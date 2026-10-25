static u8 wave_clock;
u8 entity_arcade_clock(void){return wave_clock;}
void entity_arcade_clock_advance(void){++wave_clock;}
