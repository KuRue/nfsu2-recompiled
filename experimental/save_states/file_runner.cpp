#include "file_resources.h"
#include <iostream>
using namespace file_snapshot;
int main(int argc,char** argv){try{
    require(argc==4,"usage: file_runner capture|restore root state");
    Fd root(::open(argv[2],O_RDONLY|O_DIRECTORY|O_CLOEXEC|O_NOFOLLOW));require(root.value>=0,"Cannot open root");
    if(std::string(argv[1])=="capture"){
        auto fd=openRelative(root.value,"data/input.bin");auto content=contents(fd.value);
        auto save=openRelative(root.value,"data/save.bin");auto saved=contents(save.value);
        std::vector<File> files{{100,3,0,"data/input.bin",content},{101,9,0,"data/input.bin",content},{102,65536,0,"data/input.bin",content},{103,4,1,"data/save.bin",saved},{104,0,0,"data/save.bin",saved}};
        writeAtomicBytes(argv[3],encode(files));
    }else{
        require(std::string(argv[1])=="restore","Unknown command");
        // Force a different host descriptor layout: descriptors are never saved.
        std::vector<Fd> ballast;for(int i=0;i<17;++i)ballast.emplace_back(::dup(root.value));
        auto files=restore(root.value,decode(readBytes(argv[3])));
        require(files.size()==5,"Wrong file count");std::string result;
        for(size_t i=0;i<3;++i){auto& f=files[i];uint8_t bytes[4]{};auto n=::read(f.fd.value,bytes,4);require(n>=0,"Read failed");result+=std::to_string(f.id)+":"+std::string(bytes,bytes+n)+";";}
        require(result=="100:defg;101:jklm;102:;","Wrong restored positions");
        require(::lseek(files[0].fd.value,0,SEEK_SET)==0,"Seek failed");uint8_t first=0;require(::read(files[0].fd.value,&first,1)==1&&first=='a',"Seek read failed");
        require(::lseek(files[1].fd.value,0,SEEK_CUR)==13,"Handles share offsets");
        // Writable handle continues at its saved offset; the read-only handle sees the same file.
        require(::write(files[3].fd.value,"XY",2)==2,"Write failed");uint8_t view[8]{};
        require(::pread(files[4].fd.value,view,8,0)==8&&std::string(view,view+8)=="SAVEXY23","Writable handle not restored");
        std::cout<<result<<" independent offsets and seek OK\n";
    }
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
