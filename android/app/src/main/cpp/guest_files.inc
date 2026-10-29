std::string guestWorkingDirectory="C:\\nfsu2";
void logicalDrives(){ret(4,0);}
void driveType(){auto path=lower(guestString(arg(0)));ret(path.empty()||path.rfind("c:",0)==0?3:1,1);}
uint32_t fileError(){switch(errno){case ENOENT:return 2;case EACCES:case EPERM:return 5;case EEXIST:return 80;case ENOSPC:return 112;case EBADF:return 6;default:return 1;}}
std::filesystem::path gamePath(std::string guest){
    std::replace(guest.begin(),guest.end(),'\\','/');auto normalized=lower(guest);
    if(normalized=="c:/nfsu2")guest.clear();
    else if(normalized.rfind("c:/nfsu2/",0)==0)guest=guest.substr(9);
    else if(guest.find(':')!=std::string::npos||guest.rfind("//",0)==0)throw GuestStop("Unsupported guest path: "+guest);
    else {if(!guest.empty()&&guest.front()=='/')guest.erase(0,1);else{auto current=guestWorkingDirectory.substr(8);std::replace(current.begin(),current.end(),'\\','/');if(!current.empty())guest=current+"/"+guest;}}
    auto relative=std::filesystem::path(guest).lexically_normal();for(const auto& part:relative)if(part=="..")throw GuestStop("Guest path leaves nfsu2: "+guest);
    // Preview never reads the user's Windows profile folders.
    if(launcherPreviewMode() && !relative.empty()) {
        auto first=lower(relative.begin()->string());
        if(first=="documents"||first=="appdata")
            return std::filesystem::path(std::getenv("NFSU2_PREVIEW_PROFILE"))/relative;
    }
    auto result=std::filesystem::path(hostGameRoot);
    for(const auto& part:relative){if(part==".")continue;std::error_code error;bool found=false;
        for(auto& entry:std::filesystem::directory_iterator(result,error))if(lower(entry.path().filename().string())==lower(part.string())){result=entry.path();found=true;break;}
        if(!found)result/=part;
    }
    auto root=std::filesystem::weakly_canonical(hostGameRoot);auto resolved=std::filesystem::weakly_canonical(result);
    auto check=resolved.lexically_relative(root);for(const auto& part:check)if(part=="..")throw GuestStop("Guest symlink leaves data root");return resolved;
}
std::map<uint32_t,int> fileHandles;uint32_t nextFileHandle=0xf0060000;
// The game skips its CD-drive check when the game folder holds a file named
// "foobar" (its developer switch, 0x0057CAC0("foobar")). Copies made without
// it showed the insert-disc MessageBoxA. Present it as an empty file when absent.
bool discCheckSwitch(const std::filesystem::path& path){
    std::error_code error;
    return lower(path.filename().string())=="foobar"&&!std::filesystem::exists(path,error)&&
        std::filesystem::weakly_canonical(path.parent_path())==std::filesystem::weakly_canonical(hostGameRoot);
}
// Where each open handle came from, for a capture: path relative to the game root and access.
struct FileOrigin{std::string relative;uint32_t access=0;};
std::map<uint32_t,FileOrigin> fileOrigins;
#if defined(__ANDROID__)
// Race intro assets are opened and read synchronously on the guest thread. The
// 118 MB track .BUN alone can block a frame for over half a second. Warm the
// page cache from a low-priority worker as soon as the file is opened, so the
// guest's normal read() hits resident pages instead of waiting on storage.
std::mutex prefetchGuard;
std::map<std::string,std::chrono::steady_clock::time_point> prefetchedAssets;
std::atomic<int> prefetchRunning{0};
void prefetchLargeAsset(const std::filesystem::path& path){
    std::error_code error;auto size=std::filesystem::file_size(path,error);
    if(error||size<8ull*1024*1024||size>512ull*1024*1024)return;
    auto extension=lower(path.extension().string());
    if(extension!=".bun"&&extension!=".big"&&extension!=".pak"&&extension!=".lzc")return;
    const auto now=std::chrono::steady_clock::now();std::string key=path.string();
    {
        std::lock_guard<std::mutex> lock(prefetchGuard);
        auto found=prefetchedAssets.find(key);
        if(found!=prefetchedAssets.end()&&now-found->second<std::chrono::seconds(20))return;
        if(prefetchRunning.load()>=2)return;
        prefetchedAssets[key]=now;++prefetchRunning;
    }
    NFS_RUNTIME_LOG(ANDROID_LOG_INFO,"NFSU2","Asset prefetch start %s (%.1f MiB)",path.filename().string().c_str(),size/1048576.0);
    std::thread([path]{
        auto started=std::chrono::steady_clock::now();setpriority(PRIO_PROCESS,0,10);
        int descriptor=::open(path.c_str(),O_RDONLY|O_CLOEXEC);
        if(descriptor>=0){
#ifdef POSIX_FADV_WILLNEED
            posix_fadvise(descriptor,0,0,POSIX_FADV_WILLNEED);
#endif
            std::vector<uint8_t> buffer(4u*1024u*1024u);off_t offset=0;
            while(::pread(descriptor,buffer.data(),buffer.size(),offset)>0)offset+=off_t(buffer.size());
            ::close(descriptor);
        }
        auto elapsed=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-started).count();
        NFS_RUNTIME_LOG(ANDROID_LOG_INFO,"NFSU2","Asset prefetch done %s %lld ms",path.filename().string().c_str(),(long long)elapsed);
        --prefetchRunning;
    }).detach();
}
// Remember the last track bundle the game streamed and warm it on the next
// launch. The file is small and keeps the 118 MB read out of the intro.
std::string lastTrackWritten;
void rememberTrackAsset(const std::filesystem::path& path){
    if(launcherPreviewMode())return;
    try{
        auto relative=path.lexically_relative(std::filesystem::weakly_canonical(hostGameRoot));
        auto first=relative.begin();
        if(first==relative.end()||lower(first->string())!="tracks")return;
        if(lower(path.extension().string())!=".bun")return;
        auto value=relative.generic_string();if(value==lastTrackWritten)return;lastTrackWritten=value;
        std::ofstream out(std::filesystem::path(hostGameRoot)/".android_last_track",std::ios::binary|std::ios::trunc);
        out<<value;
    }catch(...){}
}
void warmLastTrackAsset(){
    try{
        std::ifstream in(std::filesystem::path(hostGameRoot)/".android_last_track",std::ios::binary);
        std::string relative;std::getline(in,relative);if(relative.empty())return;
        auto root=std::filesystem::weakly_canonical(hostGameRoot);
        auto path=std::filesystem::weakly_canonical(root/relative);
        for(const auto& part:path.lexically_relative(root))if(part=="..")return;
        prefetchLargeAsset(path);
    }catch(...){}
}
#endif
bool closeGuestFile(uint32_t handle){auto file=fileHandles.find(handle);if(file==fileHandles.end())return false;::close(file->second);fileHandles.erase(file);fileOrigins.erase(handle);return true;}
void fileType(){
    uint32_t handle=arg(0);
    if(handle>=0xf0000200u&&handle<=0xf0000208u){ret(2,1);return;}
    if(fileHandles.find(handle)!=fileHandles.end()){ret(1,1);return;}
    lastError=6;ret(0,1);
}
void longPath(){
    auto path=gamePath(guestString(arg(0)));std::error_code error;if(!std::filesystem::exists(path,error)){lastError=2;ret(0,3);return;}
    auto relative=path.lexically_relative(std::filesystem::weakly_canonical(hostGameRoot)).string();
    std::string result="C:\\nfsu2";if(relative!="."&&!relative.empty())result+="\\"+relative;std::replace(result.begin(),result.end(),'/','\\');
    if(arg(2)<=result.size()){ret(result.size()+1,3);return;}std::memcpy(ptr(arg(1),result.size()+1),result.c_str(),result.size()+1);ret(result.size(),3);
}
// The boot movies, swapped for the game's own one-frame blank.vp6 when "Intro movies" is Skip.
// The tutorial and story movies are untouched.
std::atomic<bool> skipIntroMovies{false};
static std::filesystem::path skipIntroMovie(const std::filesystem::path& path){
    if(!skipIntroMovies.load(std::memory_order_relaxed))return path;
    auto name=path.filename().string();for(auto& c:name)c=char(std::tolower(static_cast<unsigned char>(c)));
    if(name!="ealogo.vp6"&&name!="thx_logo.vp6"&&name!="psa.vp6"&&name!="fmvopening.vp6")return path;
    auto blank=path.parent_path()/"blank.vp6";
    return std::filesystem::exists(blank)?blank:path;
}
void createFile(){
    auto path=skipIntroMovie(gamePath(guestString(arg(0))));uint32_t access=arg(1),disposition=arg(4),flags=arg(5);
    if(!launcherPreviewAllowsOpen(access,disposition)){lastError=5;ret(UINT32_MAX,7);return;}
    if(flags&0x40000000u)throw GuestStop("Overlapped CreateFile requires asynchronous file service");
    int options=access&0x40000000u?(access&0x80000000u?O_RDWR:O_WRONLY):O_RDONLY;
    switch(disposition){case 1:options|=O_CREAT|O_EXCL;break;case 2:options|=O_CREAT|O_TRUNC;break;case 3:break;case 4:options|=O_CREAT;break;case 5:options|=O_TRUNC;break;default:lastError=87;ret(UINT32_MAX,7);return;}
    bool existed=std::filesystem::exists(path);
    int descriptor=disposition==3&&!(access&0x40000000u)&&discCheckSwitch(path) ? ::open("/dev/null",O_RDONLY|O_CLOEXEC) : ::open(path.c_str(),options|O_CLOEXEC,0600);
    NFS_RUNTIME_LOG(ANDROID_LOG_INFO,"NFSU2","Guest file %s access=%x disposition=%u",path.filename().string().c_str(),access,disposition);
    if(descriptor<0){lastError=fileError();ret(UINT32_MAX,7);return;}uint32_t handle=nextFileHandle++;fileHandles.emplace(handle,descriptor);
    fileOrigins[handle]=FileOrigin{path.lexically_relative(std::filesystem::weakly_canonical(hostGameRoot)).generic_string(),access};
#if defined(__ANDROID__)
    if(options==O_RDONLY){prefetchLargeAsset(path);rememberTrackAsset(path);}
#endif
    lastError=(disposition==2||disposition==4)&&existed?183:0;ret(handle,7);
}
#ifdef NFS_RESUMABLE
// Restore: reopen each captured handle by path with its access, at its position. Never creates
// or truncates, so a career or profile file keeps its newest contents (saves are not rolled back).
// A missing file fails the restore rather than leaving the game a dead handle.
[[maybe_unused]] void restoreFiles(const std::vector<nfs_capture::FileImage>& files){
    for(auto& [handle,descriptor]:fileHandles)::close(descriptor);
    fileHandles.clear();fileOrigins.clear();
    uint32_t next=0xf0060000u;
    for(const auto& file:files){
        auto path=gamePath(file.path);
        int options=file.access&0x40000000u?(file.access&0x80000000u?O_RDWR:O_WRONLY):O_RDONLY;
        int descriptor=::open(path.c_str(),options|O_CLOEXEC);
        if(descriptor<0)throw GuestStop("Restore could not reopen "+file.path);
        off_t size=::lseek(descriptor,0,SEEK_END);
        // A file that shrank since the capture (a save rewritten shorter) keeps its end, not a hole.
        off_t target=off_t(std::min<uint64_t>(file.offset,uint64_t(size<0?0:size)));
        if(::lseek(descriptor,target,SEEK_SET)<0){::close(descriptor);throw GuestStop("Restore could not seek "+file.path);}
        fileHandles.emplace(file.handle,descriptor);fileOrigins[file.handle]=FileOrigin{file.path,file.access};
        next=std::max(next,file.handle+1);
    }
    nextFileHandle=next;
}
#endif
void readFile(){
    auto file=fileHandles.find(arg(0));if(file==fileHandles.end()){lastError=6;ret(0,5);return;}if(arg(4))throw GuestStop("Overlapped ReadFile pending");
    if(arg(3))write32(arg(3),0);
    auto started=std::chrono::steady_clock::now();
    ssize_t count;do{count=::read(file->second,ptr(arg(1),arg(2)),arg(2));}while(count<0&&errno==EINTR);
    auto elapsed=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-started).count();
    if(elapsed>=40||arg(2)>=1024u*1024u){auto origin=fileOrigins.find(arg(0));
        NFS_RUNTIME_LOG(ANDROID_LOG_INFO,"NFSU2","Guest read %s %u bytes %lld ms",origin!=fileOrigins.end()?origin->second.relative.c_str():"?",arg(2),(long long)elapsed);}
    if(count<0){lastError=fileError();ret(0,5);return;}if(arg(3))write32(arg(3),count);ret(1,5);
}
void writeFile(){
    if(arg(4))throw GuestStop("Overlapped WriteFile pending");
    if(!arg(3)||!memory->writable(arg(3),4)){lastError=87;ret(0,5);return;}write32(arg(3),0);
    auto file=fileHandles.find(arg(0));if(file==fileHandles.end()){lastError=6;ret(0,5);return;}
    if(!memory->readable(arg(1),arg(2))){lastError=1784;ret(0,5);return;}
    ssize_t count;do{count=::write(file->second,ptr(arg(1),arg(2)),arg(2));}while(count<0&&errno==EINTR);
    if(count<0){lastError=fileError();ret(0,5);return;}write32(arg(3),uint32_t(count));ret(1,5);
}
void flushFileBuffers(){auto file=fileHandles.find(arg(0));if(file==fileHandles.end()){lastError=6;ret(0,1);return;}
    int result;do{result=::fsync(file->second);}while(result&&errno==EINTR);if(result){lastError=fileError();ret(0,1);return;}ret(1,1);
}
void setEndOfFile(){auto file=fileHandles.find(arg(0));if(file==fileHandles.end()){lastError=6;ret(0,1);return;}
    auto position=::lseek(file->second,0,SEEK_CUR);if(position<0||::ftruncate(file->second,position)){lastError=fileError();ret(0,1);return;}ret(1,1);
}
void fileSize(){auto file=fileHandles.find(arg(0));struct stat status{};if(file==fileHandles.end()){lastError=6;ret(UINT32_MAX,2);return;}if(fstat(file->second,&status)){lastError=fileError();ret(UINT32_MAX,2);return;}uint64_t size=status.st_size;if(arg(1))write32(arg(1),size>>32);lastError=0;ret(size,2);}
void fileAttributes(){auto path=gamePath(guestString(arg(0)));struct stat status{};if(discCheckSwitch(path)){ret(0x20,1);return;}if(stat(path.c_str(),&status)){lastError=fileError();ret(UINT32_MAX,1);return;}ret(S_ISDIR(status.st_mode)?0x10:0x20,1);}
void setFilePointer(){
    auto file=fileHandles.find(arg(0));if(file==fileHandles.end()){lastError=6;ret(UINT32_MAX,4);return;}
    uint32_t method=arg(3),high=arg(2);if(method>2){lastError=87;ret(UINT32_MAX,4);return;}
    int64_t distance=int32_t(arg(1));if(high){uint64_t bits=(uint64_t(read32(high))<<32)|arg(1);std::memcpy(&distance,&bits,8);}
    off_t result=::lseek(file->second,off_t(distance),method==0?SEEK_SET:method==1?SEEK_CUR:SEEK_END);
    if(result<0){lastError=errno==EINVAL?131:fileError();ret(UINT32_MAX,4);return;}
    if(!high&&uint64_t(result)>UINT32_MAX){lastError=87;ret(UINT32_MAX,4);return;}
    if(high)write32(high,uint64_t(result)>>32);lastError=0;ret(uint32_t(result),4);
}
// Keep Windows profile folders alongside the user's game data on Android.
void shellFolderPath(){
    uint32_t folder=arg(1),out=arg(4);if(!out||!memory->writable(out,260)){ret(0x80070057u,5);return;}
    *static_cast<char*>(ptr(out,1))=0;
    if(arg(2)||arg(3)>1||(folder&~0xc0ffu)){ret(0x80070057u,5);return;}
    std::string suffix;switch(folder&0xff){case 0x1c:suffix="AppData\\Local";break;case 0x1a:suffix="AppData\\Roaming";break;case 5:suffix="Documents";break;default:ret(0x80070057u,5);return;}
    std::string logical="C:\\nfsu2\\"+suffix;auto path=gamePath(logical);std::error_code error;
    if((folder&0x8000)&&!launcherPreviewMode())std::filesystem::create_directories(path,error);
    if(error){ret(0x80070005u,5);return;}
    if(!(folder&0x4000)&&!std::filesystem::is_directory(path,error)){ret(0x80070003u,5);return;}
    std::memcpy(ptr(out,logical.size()+1),logical.c_str(),logical.size()+1);
    NFS_RUNTIME_LOG(ANDROID_LOG_INFO,"NFSU2","Guest profile folder %s",logical.c_str());ret(0,5);
}
struct GuestFileSearch {std::vector<std::filesystem::path> paths;size_t next=0;};
std::map<uint32_t,GuestFileSearch> fileSearches;uint32_t nextSearchHandle=0xf0070000;
bool filePatternMatches(std::string pattern,std::string name){
    pattern=lower(pattern);name=lower(name);if(pattern=="*.*")pattern="*";
    size_t p=0,n=0,star=std::string::npos,retry=0;
    while(n<name.size()){
        if(p<pattern.size()&&(pattern[p]=='?'||pattern[p]==name[n])){++p;++n;}
        else if(p<pattern.size()&&pattern[p]=='*'){star=p++;retry=n;}
        else if(star!=std::string::npos){p=star+1;n=++retry;}else return false;
    }while(p<pattern.size()&&pattern[p]=='*')++p;return p==pattern.size();
}
bool writeFindData(uint32_t out,const std::filesystem::path& path){
    if(!out||!memory->writable(out,320)){lastError=87;return false;}
    struct stat status{};if(::stat(path.c_str(),&status)){lastError=fileError();return false;}
    auto name=path.filename().string();if(name.size()>=260){lastError=206;return false;}
    std::memset(ptr(out,320),0,320);write32(out,S_ISDIR(status.st_mode)?0x10:0x20);
    auto time=[&](uint32_t offset,int64_t seconds){uint64_t ticks=(seconds+11644473600ll)*10000000ull;std::memcpy(ptr(out+offset,8),&ticks,8);};
    time(4,status.st_ctime);time(12,status.st_atime);time(20,status.st_mtime);
    uint64_t size=S_ISDIR(status.st_mode)?0:uint64_t(status.st_size);write32(out+28,size>>32);write32(out+32,uint32_t(size));
    std::memcpy(ptr(out+44,name.size()+1),name.c_str(),name.size()+1);return true;
}
void findFirstFile(){
    auto requested=guestString(arg(0));std::replace(requested.begin(),requested.end(),'\\','/');
    if(requested.empty()||requested.back()=='/'){lastError=123;ret(UINT32_MAX,2);return;}
    auto separator=requested.find_last_of('/');auto pattern=requested.substr(separator==std::string::npos?0:separator+1);
    auto directory=gamePath(separator==std::string::npos?".":requested.substr(0,separator));
    std::error_code error;GuestFileSearch search;
    if(std::filesystem::is_directory(directory,error))for(const char* dot:{".",".."})if(filePatternMatches(pattern,dot))search.paths.push_back(directory/dot);
    for(auto& entry:std::filesystem::directory_iterator(directory,error)){
        if(!filePatternMatches(pattern,entry.path().filename().string()))continue;
        // Apply the same containment check used for ordinary file opens.
        auto logical="C:/nfsu2/"+entry.path().lexically_relative(std::filesystem::weakly_canonical(hostGameRoot)).string();
        gamePath(logical);search.paths.push_back(entry.path());
    }
    if(error){lastError=error==std::errc::no_such_file_or_directory?3:5;ret(UINT32_MAX,2);return;}
    if(search.paths.empty()){lastError=2;ret(UINT32_MAX,2);return;}
    std::sort(search.paths.begin(),search.paths.end());
    if(!writeFindData(arg(1),search.paths.front())){ret(UINT32_MAX,2);return;}
    search.next=1;uint32_t handle=nextSearchHandle++;fileSearches.emplace(handle,std::move(search));ret(handle,2);
}
void findNextFile(){auto found=fileSearches.find(arg(0));if(found==fileSearches.end()){lastError=6;ret(0,2);return;}
    auto& search=found->second;if(search.next==search.paths.size()){lastError=18;ret(0,2);return;}
    if(!writeFindData(arg(1),search.paths[search.next])){ret(0,2);return;}++search.next;ret(1,2);
}
void findClose(){if(!fileSearches.erase(arg(0))){lastError=6;ret(0,1);return;}ret(1,1);}
void createDirectory(){
    if(launcherPreviewMode()){lastError=5;ret(0,2);return;}if(arg(1))throw GuestStop("CreateDirectory security attributes pending");auto path=gamePath(guestString(arg(0)));
    if(::mkdir(path.c_str(),0700)){lastError=errno==EEXIST?183:fileError();ret(0,2);return;}ret(1,2);
}
void fullPathName(){
    auto requested=guestString(arg(0));if(requested.empty()){lastError=123;ret(0,4);return;}
    auto path=gamePath(requested);auto relative=path.lexically_relative(std::filesystem::weakly_canonical(hostGameRoot)).string();
    std::string logical="C:\\nfsu2";if(relative!="."&&!relative.empty())logical+="\\"+relative;
    std::replace(logical.begin(),logical.end(),'/','\\');if(requested.back()=='/'||requested.back()=='\\')logical+='\\';
    uint32_t capacity=arg(1),out=arg(2),part=arg(3);if(capacity<=logical.size()){ret(uint32_t(logical.size()+1),4);return;}
    if(!out||!memory->writable(out,logical.size()+1)||(part&&!memory->writable(part,4))){lastError=87;ret(0,4);return;}
    std::memcpy(ptr(out,logical.size()+1),logical.c_str(),logical.size()+1);
    if(part)write32(part,logical.back()=='\\'?0:out+uint32_t(logical.find_last_of('\\')+1));ret(uint32_t(logical.size()),4);
}
// DeleteFileA / MoveFileA inside the game folder (gamePath refuses anything outside it).
void deleteFile(){
    if(launcherPreviewMode()){lastError=5;ret(0,1);return;}
    auto path=gamePath(guestString(arg(0)));
    if(::unlink(path.c_str())){lastError=fileError();ret(0,1);return;}
    lastError=0;ret(1,1);
}
void removeDirectory(){
    if(launcherPreviewMode()){lastError=5;ret(0,1);return;}
    auto path=gamePath(guestString(arg(0)));
    if(::rmdir(path.c_str())){lastError=errno==ENOTEMPTY||errno==EEXIST?145:fileError();ret(0,1);return;}  // ERROR_DIR_NOT_EMPTY
    lastError=0;ret(1,1);
}
void moveFile(){
    if(launcherPreviewMode()){lastError=5;ret(0,2);return;}
    auto from=gamePath(guestString(arg(0))),to=gamePath(guestString(arg(1)));
    struct stat status{};if(!::stat(to.c_str(),&status)){lastError=183;ret(0,2);return;}  // ERROR_ALREADY_EXISTS
    if(::rename(from.c_str(),to.c_str())){lastError=fileError();ret(0,2);return;}
    lastError=0;ret(1,2);
}
// GetFileTime(handle, creation, access, write): FILETIME is 100 ns ticks since 1601.
void fileTime(){
    auto file=fileHandles.find(arg(0));struct stat status{};
    if(file==fileHandles.end()){lastError=6;ret(0,4);return;}
    if(fstat(file->second,&status)){lastError=fileError();ret(0,4);return;}
    auto ticks=[](const timespec& time){return uint64_t(time.tv_sec)*10000000ull+uint64_t(time.tv_nsec)/100u+116444736000000000ull;};
    uint64_t values[3]={ticks(status.st_mtim),ticks(status.st_atim),ticks(status.st_mtim)};  // no creation time: use the last write
    for(unsigned i=0;i<3;++i)if(arg(1+i))std::memcpy(ptr(arg(1+i),8),&values[i],8);
    lastError=0;ret(1,4);
}
const std::map<std::string,recomp_func_t> fileFunctions={
    {"GetFileTime",fileTime},{"DeleteFileA",deleteFile},{"RemoveDirectoryA",removeDirectory},{"MoveFileA",moveFile},
    {"GetFullPathNameA",fullPathName},{"WriteFile",writeFile},{"FlushFileBuffers",flushFileBuffers},{"SetEndOfFile",setEndOfFile},
    {"FindFirstFileA",findFirstFile},{"FindNextFileA",findNextFile},{"FindClose",findClose},{"CreateDirectoryA",createDirectory},
    {"SetFilePointer",setFilePointer},
    {"GetLogicalDrives",logicalDrives},{"GetDriveTypeA",driveType},
    {"GetLongPathNameA",longPath},{"CreateFileA",createFile},{"ReadFile",readFile},{"GetFileSize",fileSize},{"GetFileAttributesA",fileAttributes}
};
