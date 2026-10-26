#pragma once
// Isolated file resource proof; never used by the installed game.
#include "snapshot.h"
#include <sys/stat.h>
#include <cerrno>
#include <set>
#include <memory>
#include <map>
#include <unistd.h>
namespace file_snapshot {
using namespace snapshot_prototype;
constexpr size_t maxFile=65536;
struct Fd {
    int value=-1;
    explicit Fd(int n=-1):value(n){}
    ~Fd(){if(value>=0)::close(value);}
    Fd(const Fd&)=delete;Fd& operator=(const Fd&)=delete;
    Fd(Fd&& other)noexcept:value(other.value){other.value=-1;}
    Fd& operator=(Fd&& other)noexcept{if(this!=&other){if(value>=0)::close(value);value=other.value;other.value=-1;}return *this;}
};
struct File {uint32_t id=0,position=0,writable=0;std::string path;Bytes content;};
inline void validate(const std::vector<File>& files){
    require(files.size()<=32,"Too many files");std::set<uint32_t> ids;std::map<std::string,const Bytes*> seen;
    for(const auto& f:files){
        require(f.writable<=1,"Invalid file mode");
        // Handles on one path were captured at one instant: they must agree on its bytes.
        auto known=seen.emplace(f.path,&f.content);require(known.second||*known.first->second==f.content,"Handles disagree on file content");
        require(f.id&&ids.insert(f.id).second,"Duplicate file handle");
        require(!f.path.empty()&&f.path.size()<=1024&&f.path.front()!='/'&&f.path.find('\0')==std::string::npos,"Invalid path");
        size_t start=0;
        while(start<f.path.size()){
            auto end=f.path.find('/',start);if(end==std::string::npos)end=f.path.size();
            auto part=f.path.substr(start,end-start);
            require(!part.empty()&&part!="."&&part!="..","Unsafe path component");start=end+1;
        }
        require(f.path.back()!='/'&&f.content.size()<=maxFile&&f.position<=maxFile,"File exceeds prototype bounds");
    }
}
inline Fd openRelative(int root,const std::string& path,int mode=O_RDONLY){
    Fd directory(::dup(root));require(directory.value>=0,"Cannot duplicate root");size_t start=0;
    for(;;){
        auto end=path.find('/',start);bool last=end==std::string::npos;
        auto component=path.substr(start,last?std::string::npos:end-start);
        int fd=::openat(directory.value,component.c_str(),(last?mode:O_RDONLY|O_DIRECTORY)|O_CLOEXEC|O_NOFOLLOW|O_NONBLOCK);
        require(fd>=0,"Cannot reopen file component");
        if(last)return Fd(fd);
        ::close(directory.value);directory.value=fd;start=end+1;
    }
}
inline Bytes contents(int fd){
    struct stat st{};require(::fstat(fd,&st)==0&&S_ISREG(st.st_mode)&&st.st_size>=0&&uint64_t(st.st_size)<=maxFile,"Unsupported file type or size");
    Bytes out(size_t(st.st_size));size_t pos=0;
    while(pos<out.size()){
        auto n=::pread(fd,out.data()+pos,out.size()-pos,off_t(pos));
        if(n<0&&errno==EINTR)continue;
        require(n>0,"Cannot read complete file");pos+=size_t(n);
    }
    uint8_t extra=0;require(::pread(fd,&extra,1,off_t(out.size()))==0,"File grew during validation");return out;
}
struct OpenFile {uint32_t id;Fd fd;};
inline std::pair<Fd,std::string> parentOf(int root,const std::string& path){
    auto slash=path.rfind('/');if(slash==std::string::npos)return {Fd(::dup(root)),path};
    return {openRelative(root,path.substr(0,slash),O_RDONLY|O_DIRECTORY),path.substr(slash+1)};
}
inline void writeAll(int fd,const Bytes& bytes){
    size_t done=0;while(done<bytes.size()){auto n=::write(fd,bytes.data()+done,bytes.size()-done);if(n<0&&errno==EINTR)continue;require(n>0,"Cannot write restored file");done+=size_t(n);}
}
struct Staged {Fd directory;std::string name,temp;bool published=false;
    ~Staged(){if(!published&&directory.value>=0)::unlinkat(directory.value,temp.c_str(),0);}
};
// Writable files: the snapshot owns their bytes. Stage every replacement, validate every
// read-only file, and only then rename replacements into place (never following links).
// Read-only files must still equal their captured bytes.
inline std::vector<OpenFile> restore(int root,const std::vector<File>& files){
    validate(files);std::map<std::string,const File*> writable;
    for(const auto& f:files)if(f.writable)writable.emplace(f.path,&f);
    std::vector<std::unique_ptr<Staged>> staged;
    for(const auto& [path,file]:writable){
        auto parent=parentOf(root,path);struct stat st{};
        require(::fstatat(parent.first.value,parent.second.c_str(),&st,AT_SYMLINK_NOFOLLOW)==0&&S_ISREG(st.st_mode),"Writable target is not a regular file");
        auto item=std::make_unique<Staged>();item->directory=std::move(parent.first);item->name=parent.second;
        item->temp=".nfs-restore-"+std::to_string(::getpid())+"-"+std::to_string(file->id);
        Fd out(::openat(item->directory.value,item->temp.c_str(),O_WRONLY|O_CREAT|O_EXCL|O_CLOEXEC|O_NOFOLLOW,0600));
        require(out.value>=0,"Cannot stage restored file");staged.push_back(std::move(item));
        writeAll(out.value,file->content);require(::fsync(out.value)==0,"Cannot sync restored file");
    }
    for(const auto& f:files)if(!writable.count(f.path)){auto fd=openRelative(root,f.path);require(contents(fd.value)==f.content,"File content changed");}
    for(auto& item:staged){
        require(::renameat(item->directory.value,item->temp.c_str(),item->directory.value,item->name.c_str())==0,"Cannot publish restored file");
        item->published=true;require(::fsync(item->directory.value)==0,"Cannot sync restored directory");
    }
    std::vector<OpenFile> opened;std::map<std::string,ino_t> inode;
    for(const auto& file:files){
        auto fd=openRelative(root,file.path,file.writable?O_RDWR:O_RDONLY);
        require(contents(fd.value)==file.content,"File content changed");
        struct stat st{};require(::fstat(fd.value,&st)==0,"Cannot stat restored file");
        auto known=inode.emplace(file.path,st.st_ino);require(known.second||known.first->second==st.st_ino,"File replaced during restore");
        require(::lseek(fd.value,file.position,SEEK_SET)==off_t(file.position),"Cannot restore position");
        opened.push_back({file.id,std::move(fd)});
    }
    return opened;
}
inline Bytes encode(const std::vector<File>& files){
    validate(files);Bytes payload;word(payload,uint32_t(files.size()));
    for(const auto& f:files){word(payload,f.id);word(payload,f.position);word(payload,f.writable);word(payload,uint32_t(f.path.size()));payload.insert(payload.end(),f.path.begin(),f.path.end());word(payload,uint32_t(f.content.size()));payload.insert(payload.end(),f.content.begin(),f.content.end());}
    Bytes out{'N','F','S','F','I','L','E','S'};word(out,2);word(out,uint32_t(payload.size()));word(out,crc(payload));out.insert(out.end(),payload.begin(),payload.end());return out;
}
inline std::vector<File> decode(const Bytes& bytes){
    Reader r{bytes};require(r.take(8)==Bytes({'N','F','S','F','I','L','E','S'})&&r.word()==2,"Wrong file schema");
    auto size=r.word(),checksum=r.word();require(size<=32*(maxFile+1044)+4,"Oversized file envelope");auto payload=r.take(size);require(r.offset==bytes.size()&&crc(payload)==checksum,"Corrupt file envelope");
    Reader p{payload};auto count=p.word();require(count<=32,"Too many files");std::vector<File> files;
    for(uint32_t i=0;i<count;++i){File f;f.id=p.word();f.position=p.word();f.writable=p.word();auto length=p.word();require(length<=1024,"Oversized path");auto path=p.take(length);f.path.assign(path.begin(),path.end());length=p.word();require(length<=maxFile,"Oversized content");f.content=p.take(length);files.push_back(std::move(f));}
    require(p.offset==payload.size(),"Trailing file metadata");validate(files);return files;
}
}
