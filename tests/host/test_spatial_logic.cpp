#include <cstdio>
#include <cstdlib>
#include "saturn/spatial.h"
#define OK(x) do { if (!(x)) { std::fprintf(stderr,"FAIL %s:%d\n",__FILE__,__LINE__); std::exit(1); } } while(0)
static sat_fx16_t F(int x){return static_cast<sat_fx16_t>(x*65536);}
int main(){
 uint16_t heads[16],stamps[8],ids[8],n;
 sat_spatial_entry_t entries[32];
 sat_box2_t items[8];
 sat_spatial_t s;
 OK(sat_spatial_init(&s,heads,4,4,2,entries,32,stamps,items,8)==SAT_OK);
 sat_box2_t a={{F(2),F(2)},{F(1),F(1)}},b={{F(3),F(2)},{F(1),F(1)}},c={{F(12),F(12)},{F(1),F(1)}};
 OK(sat_spatial_insert(&s,0,&a)==SAT_OK);
 OK(sat_spatial_insert(&s,1,&b)==SAT_OK);
 OK(sat_spatial_insert(&s,2,&c)==SAT_OK);
 OK(s.entry_count>0);
 for(uint16_t i=0;i<s.entry_count;++i) OK(entries[i].cell<16u);
 OK(sat_spatial_query(&s,&a,ids,8,&n)==SAT_OK);
 OK(n==2);
 sat_spatial_pair_t pairs[4];
 OK(sat_spatial_pairs(&s,pairs,4,&n)==SAT_OK);
 OK(n==1);
 OK(pairs[0].a==0&&pairs[0].b==1);
 sat_spatial_clear(&s);
 OK(s.entry_count==0);
 OK(sat_spatial_query(&s,&a,ids,8,&n)==SAT_OK);
 OK(n==0);
 OK(sat_spatial_insert(&s,0,&a)==SAT_OK);
 s.entry_cap=0;
 OK(sat_spatial_insert(&s,1,&b)==SAT_ERR_CAPACITY);
 std::puts("PASS: test_spatial_logic.cpp");
 return 0;
}
