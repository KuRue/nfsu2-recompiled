#pragma once
// Isolated read-only resource proof; never used by the installed game.
#include "snapshot.h"
#include <sys/stat.h>
#include <cerrno>
#include <set>
namespace file_snapshot {
using namespace snapshot_prototype;
constexpr size_t maxFile=65536;
struct Fd {
    int value=-1;
    explicit Fd(int n=-1):value(n){}
    ~Fd(){if(value>=0)::close(value);}
    Fd(const Fd&)=delete;Fd& operator=(const Fd&)=delete;
    Fd(Fd&& other)noexcept:value(other.value){other.value=-1;}
};
struct File {uint32_t id=0,position=0;std::string path;Bytes content;};
inline void validate(const std::vector<File>& files){
    require(files.size()<=32,"Too many files");std::set<uint32_t> ids;
    for(const auto& f:files){
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
inline Fd openRelative(int root,const std::string& path){
    Fd directory(::dup(root));require(directory.value>=0,"Cannot duplicate root");size_t start=0;
    for(;;){
        auto end=path.find('/',start);bool last=end==std::string::npos;
        auto component=path.substr(start,last?std::string::npos:end-start);
        int fd=::openat(directory.value,component.c_str(),O_RDONLY|O_CLOEXEC|O_NOFOLLOW|O_NONBLOCK|(last?0:O_DIRECTORY));
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
inline std::vector<OpenFile> restore(int root,const std::vector<File>& files){
    validate(files);std::vector<OpenFile> staged;
    for(const auto& file:files){
        auto fd=openRelative(root,file.path);
        require(contents(fd.value)==file.content,"File content changed");
        require(::lseek(fd.value,file.position,SEEK_SET)==off_t(file.position),"Cannot restore position");
        staged.push_back({file.id,std::move(fd)});
    }
    return staged; // Only publish the complete set after all resources validate.
}
inline Bytes encode(const std::vector<File>& files){
    validate(files);Bytes payload;word(payload,uint32_t(files.size()));
    for(const auto& f:files){word(payload,f.id);word(payload,f.position);word(payload,uint32_t(f.path.size()));payload.insert(payload.end(),f.path.begin(),f.path.end());word(payload,uint32_t(f.content.size()));payload.insert(payload.end(),f.content.begin(),f.content.end());}
    Bytes out{'N','F','S','F','I','L','E','S'};word(out,1);word(out,uint32_t(payload.size()));word(out,crc(payload));out.insert(out.end(),payload.begin(),payload.end());return out;
}
inline std::vector<File> decode(const Bytes& bytes){
    Reader r{bytes};require(r.take(8)==Bytes({'N','F','S','F','I','L','E','S'})&&r.word()==1,"Wrong file schema");
    auto size=r.word(),checksum=r.word();require(size<=32*(maxFile+1040)+4,"Oversized file envelope");auto payload=r.take(size);require(r.offset==bytes.size()&&crc(payload)==checksum,"Corrupt file envelope");
    Reader p{payload};auto count=p.word();require(count<=32,"Too many files");std::vector<File> files;
    for(uint32_t i=0;i<count;++i){File f;f.id=p.word();f.position=p.word();auto length=p.word();require(length<=1024,"Oversized path");auto path=p.take(length);f.path.assign(path.begin(),path.end());length=p.word();require(length<=maxFile,"Oversized content");f.content=p.take(length);files.push_back(std::move(f));}
    require(p.offset==payload.size(),"Trailing file metadata");validate(files);return files;
}
}
