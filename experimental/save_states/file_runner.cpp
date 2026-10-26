#include "file_resources.h"
#include <iostream>
using namespace file_snapshot;
int main(int argc,char** argv){try{
    require(argc==4,"usage: file_runner capture|restore root state");
    Fd root(::open(argv[2],O_RDONLY|O_DIRECTORY|O_CLOEXEC|O_NOFOLLOW));require(root.value>=0,"Cannot open root");
    if(std::string(argv[1])=="capture"){
        auto fd=openRelative(root.value,"data/input.bin");auto content=contents(fd.value);
        std::vector<File> files{{100,3,"data/input.bin",content},{101,9,"data/input.bin",content},{102,65536,"data/input.bin",content}};
        writeAtomicBytes(argv[3],encode(files));
    }else{
        require(std::string(argv[1])=="restore","Unknown command");
        // Force a different host descriptor layout: descriptors are never saved.
        std::vector<Fd> ballast;for(int i=0;i<17;++i)ballast.emplace_back(::dup(root.value));
        auto files=restore(root.value,decode(readBytes(argv[3])));
        require(files.size()==3,"Wrong file count");std::string result;
        for(auto& f:files){uint8_t bytes[4]{};auto n=::read(f.fd.value,bytes,4);require(n>=0,"Read failed");result+=std::to_string(f.id)+":"+std::string(bytes,bytes+n)+";";}
        require(result=="100:defg;101:jklm;102:;","Wrong restored positions");
        require(::lseek(files[0].fd.value,0,SEEK_SET)==0,"Seek failed");uint8_t first=0;require(::read(files[0].fd.value,&first,1)==1&&first=='a',"Seek read failed");
        require(::lseek(files[1].fd.value,0,SEEK_CUR)==13,"Handles share offsets");
        std::cout<<result<<" independent offsets and seek OK\n";
    }
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
