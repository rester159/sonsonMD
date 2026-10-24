        {
            u16 cells[2]={65535,65535};u8 k;
            for(k=0;k<arcade_admission.count;k++)
            {
                u8 id=arcade_admission.order[k];
                if(id>=74 && id<76)cells[id-74]=arcade_e_render_request(&arcade_e.actors[id-74]);
            }
            if(arcade_e_upload(cells,704,2))
            {
                for(k=0;k<2;k++)if(cells[k]!=65535)arcade_e_ready[k]=cells[k];
            }
            else arcade_e_ready[0]=arcade_e_ready[1]=65535;
        }