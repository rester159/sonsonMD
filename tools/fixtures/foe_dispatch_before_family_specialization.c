            if(reserved)arcade_foe_step_shared(a,0,(u8)i,&arcade_world,
                            game_arcade_floor,0,arcade_weapons,reserved);
            else arcade_foe_step(a,(u8)(i>=6),(u8)(i>=6?i-6:i),&arcade_world,
                            game_arcade_floor,0,arcade_weapons);