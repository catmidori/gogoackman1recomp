// Local integration harness: no copyrighted ROM or IPS fixture is distributed.
// Link with the unchanged framework mod_runtime/text_xlate/crc32/sha256 objects.
#include "mod_runtime.h"
#include "snes_text_xlate.h"
#include "sha256.h"
#include "snes/snes.h"
#include <cassert>
#include <fstream>
#include <iostream>
#include <iterator>
#include <vector>
#include <cstring>
extern "C" { Snes *g_snes=nullptr;Ppu *g_ppu=nullptr; }
static std::vector<uint8_t> read(const char *path) {
    std::ifstream file(path,std::ios::binary);assert(file);
    return {std::istreambuf_iterator<char>(file),std::istreambuf_iterator<char>()};
}
static std::string digest(const std::vector<uint8_t>& bytes) {
    uint8_t raw[32];sha256_compute(bytes.data(),bytes.size(),raw);
    static const char hex[]="0123456789abcdef";std::string out;
    for(uint8_t b:raw){out+=hex[b>>4];out+=hex[b&15];}return out;
}
int main(int argc,char **argv) {
    assert(argc==4);
    assert(snes_mod_runtime_initialize_c(argv[3],"go-go-ackman-jp",
        "e8a33b6c8e15cf0aa60e9694471a51926fb97ad895d4714384c6abff28a33a1f"));
    auto archive=read(argv[1]);char id[128],version[64],error[512],root[4096];
    assert(snes_mod_runtime_install_blob_c(archive.data(),archive.size(),digest(archive).c_str(),
        id,sizeof(id),version,sizeof(version),error,sizeof(error)));
    assert(!strcmp(id,"ackman.translation.en") && !strcmp(version,"1.0"));
    assert(snes_mod_runtime_package_root_c(id,version,root,sizeof(root)));
    std::string table=std::string(root)+"/translation.toml";
    auto rom=read(argv[2]);Snes snes{};Cart cart{};snes.cart=&cart;g_snes=&snes;
    cart.rom=rom.data();cart.romSize=rom.size();
    assert(snes_text_xlate_init_c(table.c_str(),"en"));snes_text_xlate_on_frame_c();
    assert(digest(rom)=="4dac0fca32d47e1d6929677b9f6ab8f6840a0504c9d20dc534a4c8cf41288f74");
    // Reapplying English is idempotent; disabling restores the guarded stock bytes.
    snes_text_xlate_set_language_c("en");snes_text_xlate_on_frame_c();
    assert(digest(rom)=="4dac0fca32d47e1d6929677b9f6ab8f6840a0504c9d20dc534a4c8cf41288f74");
    snes_text_xlate_set_language_c("off");snes_text_xlate_on_frame_c();
    assert(digest(rom)=="e8a33b6c8e15cf0aa60e9694471a51926fb97ad895d4714384c6abff28a33a1f");
    rom[0x3ad30]^=1;uint8_t changed=rom[0x3ad30];
    snes_text_xlate_set_language_c("en");snes_text_xlate_on_frame_c();
    assert(rom[0x3ad30]==changed); // Source mismatch must not be overwritten.
    snes_text_xlate_shutdown_c();g_snes=nullptr;
    std::cout<<"PASS native archive install, metadata parsing, guarded ROM hooks, idempotence and restoration\n";
}
