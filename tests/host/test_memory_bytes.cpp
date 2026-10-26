#include "src/core/memory/bytes.h"
#include <cassert>
#include <cstring>
#include <cstdio>
#include <initializer_list>
int main() {
    alignas(16) unsigned char source[1100],actual[1100],expected[1100];
    for(unsigned i=0;i<sizeof(source);++i)source[i]=(unsigned char)(i*37u+19u);
    for(unsigned d=0;d<8;++d)for(unsigned s=0;s<8;++s)for(unsigned n=0;n<=1024;++n) {
        std::memset(actual,0xA5,sizeof(actual));std::memset(expected,0xA5,sizeof(expected));
        std::memcpy(expected+16+d,source+16+s,n);
        assert(saturn_copy_bytes(actual+16+d,source+16+s,n)==actual+16+d);
        assert(std::memcmp(actual,expected,sizeof(actual))==0);
    }
    for(unsigned d=0;d<8;++d)for(unsigned n=0;n<=1024;++n)
        for(int c : {0,-1,0x123,0x80}) {
            std::memset(actual,0xA5,sizeof(actual));std::memset(expected,0xA5,sizeof(expected));
            std::memset(expected+16+d,c,n);
            assert(saturn_fill_bytes(actual+16+d,c,n)==actual+16+d);
            assert(std::memcmp(actual,expected,sizeof(actual))==0);
        }
    std::puts("memory bytes: all alignments, lengths, tails and fill truncation OK");
}
