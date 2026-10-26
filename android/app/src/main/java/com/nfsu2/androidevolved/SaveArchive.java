package com.nfsu2.androidevolved;

import java.io.*;
import java.nio.file.*;
import java.util.*;
import java.util.zip.*;

/** Game-data-free ZIP format and staged restore; caller must hold SaveAccess. */
final class SaveArchive {
    static final int MAX_FILES=256;
    static final long MAX_BYTES=32L*1024*1024;
    private static final String MARKER="NFSU2-save-backup-v1\n";
    static final class Restored {
        final int files; final File backup;
        Restored(int files,File backup){this.files=files;this.backup=backup;}
    }
    private static String checked(String name) throws IOException {
        if(name.isEmpty()||name.startsWith("/")||name.contains("\\")||name.contains(":")||name.indexOf('\0')>=0)
            throw new IOException("Invalid save path");
        String[] parts=name.split("/",-1);
        if(parts.length>8)throw new IOException("Save path too deep");
        for(String part:parts)if(part.isEmpty()||part.equals(".")||part.equals(".."))throw new IOException("Invalid save path");
        return name;
    }
    private static List<Path> files(File root) throws IOException {
        List<Path> result=new ArrayList<>();
        if(!root.exists())return result;
        if(Files.isSymbolicLink(root.toPath())||!root.isDirectory())throw new IOException("Invalid save folder");
        try(java.util.stream.Stream<Path> walk=Files.walk(root.toPath(),8)){
            Iterator<Path> it=walk.iterator();long bytes=0;
            while(it.hasNext()){
                Path path=it.next();if(Files.isSymbolicLink(path))throw new IOException("Linked save files are not supported");
                if(path.equals(root.toPath()))continue;
                checked(root.toPath().relativize(path).toString().replace(File.separatorChar,'/'));
                if(Files.isDirectory(path))continue;
                if(!Files.isRegularFile(path))throw new IOException("Invalid save file");
                bytes+=Files.size(path);result.add(path);
                if(bytes>MAX_BYTES||result.size()>MAX_FILES)throw new IOException("Save folder exceeds backup limits");
            }
        }
        result.sort(Comparator.comparing(Path::toString));return result;
    }
    static File backup(File root,File destination) throws IOException {
        recover(root);
        List<Path> paths=files(root);
        if(paths.isEmpty())throw new IOException("No career saves found. Save a profile in the game first.");
        Files.createDirectories(destination.toPath());
        File partial=File.createTempFile(".nfsu2-saves-", ".partial",destination);
        File complete=new File(destination,"nfsu2-saves-"+new java.text.SimpleDateFormat("yyyyMMdd-HHmmss",Locale.ROOT).format(new Date())+"-"+UUID.randomUUID().toString().substring(0,8)+".zip");
        boolean success=false;
        try{
            try(ZipOutputStream zip=new ZipOutputStream(new FileOutputStream(partial))){
                zip.putNextEntry(new ZipEntry("format.txt"));zip.write(MARKER.getBytes(java.nio.charset.StandardCharsets.UTF_8));zip.closeEntry();
                byte[] buffer=new byte[8192];long total=0;
                for(Path path:paths){
                    zip.putNextEntry(new ZipEntry("profiles/"+checked(root.toPath().relativize(path).toString().replace(File.separatorChar,'/'))));
                    try(InputStream input=Files.newInputStream(path)){int n;while((n=input.read(buffer))!=-1){total+=n;if(total>MAX_BYTES)throw new IOException("Save data changed or exceeds backup limits");zip.write(buffer,0,n);}}
                    zip.closeEntry();
                }
            }
            // Read back CRCs and bounds before publishing a completed backup.
            read(partial,null);
            Files.move(partial.toPath(),complete.toPath());success=true;return complete;
        }finally{if(!success)Files.deleteIfExists(partial.toPath());}
    }
    private static int read(File archive,File target) throws IOException {
        boolean marker=false;int count=0,entries=0;long total=0;Set<String> names=new HashSet<>();
        try(ZipInputStream zip=new ZipInputStream(new FileInputStream(archive))){
            ZipEntry entry;byte[] buffer=new byte[8192];
            while((entry=zip.getNextEntry())!=null){
                if(++entries>MAX_FILES+1||entry.isDirectory()||!names.add(entry.getName().toLowerCase(Locale.ROOT)))throw new IOException("Invalid or duplicate archive entry");
                String name=entry.getName();
                if(name.equals("format.txt")){
                    ByteArrayOutputStream text=new ByteArrayOutputStream();int n;
                    while((n=zip.read(buffer))!=-1){if(text.size()+n>128)throw new IOException("Invalid backup format");text.write(buffer,0,n);}
                    if(!MARKER.equals(text.toString("UTF-8")))throw new IOException("This is not an NFSU2 save backup");marker=true;
                }else{
                    if(!name.startsWith("profiles/"))throw new IOException("Unexpected file in backup");
                    String relative=checked(name.substring(9));count++;
                    File output=target==null?null:new File(target,relative);
                    if(output!=null)Files.createDirectories(output.toPath().getParent());
                    try(OutputStream out=output==null?OutputStream.nullOutputStream():new FileOutputStream(output)){
                        int n;while((n=zip.read(buffer))!=-1){total+=n;if(total>MAX_BYTES)throw new IOException("Backup exceeds size limit");out.write(buffer,0,n);}
                    }
                }
                zip.closeEntry();
            }
        }
        if(!marker||count==0)throw new IOException("No saves in this NFSU2 backup");return count;
    }
    static int inspect(File archive) throws IOException {return read(archive,null);}
    private static File journal(File root){return new File(root.getParentFile(),".nfsu2-save-restore-journal");}
    static void recover(File root) throws IOException {
        File journal=journal(root);if(!journal.exists())return;
        String name;
        try(DataInputStream input=new DataInputStream(new FileInputStream(journal))){name=input.readUTF();}
        if(!name.matches("\\.nfsu2-before-[a-f0-9-]{36}"))throw new IOException("Invalid save recovery record");
        File previous=new File(root.getParentFile(),name);
        if(!root.exists()&&previous.isDirectory())Files.move(previous.toPath(),root.toPath());
        if(!root.isDirectory())throw new IOException("Save recovery needs attention; keep your backup files");
        Files.delete(journal.toPath());
    }
    static Restored restore(File archive,File root,File backups) throws IOException {
        recover(root);inspect(archive); // Validate everything before touching live saves.
        Files.createDirectories(root.getParentFile().toPath());
        File staged=Files.createTempDirectory(root.getParentFile().toPath(),".nfsu2-restore-").toFile();
        File recovery=null;
        try{
            // Merge: profiles absent from the imported ZIP are kept.
            for(Path source:files(root)){
                Path target=staged.toPath().resolve(root.toPath().relativize(source));Files.createDirectories(target.getParent());Files.copy(source,target);
            }
            int count=read(archive,staged);files(staged); // Also bound the merged result.
            if(!files(root).isEmpty())recovery=backup(root,backups);
            File previous=new File(root.getParentFile(),".nfsu2-before-"+UUID.randomUUID());
            try(FileOutputStream bytes=new FileOutputStream(journal(root));DataOutputStream out=new DataOutputStream(bytes)){
                out.writeUTF(previous.getName());out.flush();bytes.getFD().sync();
            }
            try{
                if(root.exists())Files.move(root.toPath(),previous.toPath());
                Files.move(staged.toPath(),root.toPath());
                Files.delete(journal(root).toPath());
            }catch(IOException error){recover(root);throw error;}
            // Previous folder deliberately retained as a second recovery copy.
            return new Restored(count,recovery);
        }finally{
            if(staged.exists())try(java.util.stream.Stream<Path> paths=Files.walk(staged.toPath())){
                for(Path path:(Iterable<Path>)paths.sorted(Comparator.reverseOrder())::iterator)Files.delete(path);
            }
        }
    }
}
