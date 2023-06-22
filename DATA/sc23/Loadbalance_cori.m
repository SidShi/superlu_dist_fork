clear all
clc
close all

format shortE

%% read the reference data 

SOLVE_SLU=[];

cmap='parula';
% cmap='hot';

nrhs = 1;
code = 'superlu_dist_new3Dsolve_03_25_23';
% mats={'s1_mat_0_253872.bin' 's2D9pt2048.rua' 'nlpkkt80.bin' 'Li4324.bin' 'Ga19As19H42.bin' 'ldoor.mtx'};
mats={'s2D9pt2048.rua'};

% mats={'nlpkkt80.bin'};




mats_nopost={};
for ii=1:length(mats)
    tmp = mats{ii};
    k = strfind(tmp,'.');
    mats_nopost{1,ii}=tmp(1:k-1);
end



% npzs = [1 2 4 8 16 32 ];
% nprows_all = [64 32 16 16 8 8 ];
% npcols_all = [32 32 32 16 16 8 ];


npzs = [1 2 4 8 16 32 ];
nprows_all = [32 16 16 8 8 4  ];
npcols_all = [32 32 16 16 8 8  ];

% % 
% npzs = [1 2 4 8 16 32 ];
% nprows_all = [8 8 4 4 2 2 ];
% npcols_all = [16 8 8 4 4 2 ];


lineColors = line_colors(length(npzs)+1);


for nm=1:length(mats)
    mat = mats{nm};

    SOLVE_SLU = zeros(length(npzs),length(nprows_all(:,1)),3);
    SOLVE_COMM_Z_OLD = zeros(length(npzs),length(nprows_all(:,1)));
    SOLVE_COMP_2D_OLD = zeros(length(npzs),length(nprows_all(:,1)));
    SOLVE_COMM_2D_OLD = zeros(length(npzs),length(nprows_all(:,1)));
    SOLVE_L_Z_OLD_minmax = zeros(length(npzs),length(nprows_all(:,1)),3);
    SOLVE_U_Z_OLD_minmax = zeros(length(npzs),length(nprows_all(:,1)),3);
    SOLVE_COMM_Z_OLD_minmax = zeros(length(npzs),length(nprows_all(:,1)),3);

    SOLVE_COMM_Z_NEWEST = zeros(length(npzs),length(nprows_all(:,1)));
    SOLVE_COMP_2D_NEWEST = zeros(length(npzs),length(nprows_all(:,1)));
    SOLVE_COMM_2D_NEWEST = zeros(length(npzs),length(nprows_all(:,1)));
    SOLVE_L_Z_NEWEST_minmax = zeros(length(npzs),length(nprows_all(:,1)),3);
    SOLVE_U_Z_NEWEST_minmax = zeros(length(npzs),length(nprows_all(:,1)),3);
    SOLVE_COMM_Z_NEWEST_minmax = zeros(length(npzs),length(nprows_all(:,1)),3);

    for zz=1:length(npzs)
        npz=npzs(zz);  
        nprows=nprows_all(:,zz);
        npcols=npcols_all(:,zz);

        Solve_SLU_OLD=zeros(1,length(nprows));
        Solve_SLU_NEWEST=zeros(1,length(nprows));
        Flops_SLU_OLD=zeros(1,length(nprows));
        Flops_SLU_NEWEST=zeros(1,length(nprows));
        
        Solve_L_OLD=zeros(1,length(nprows));
        Solve_L_OLD_minmax=zeros(2,length(nprows));
        Solve_L_COMP_OLD=zeros(1,length(nprows));
        Solve_L_COMM_OLD=zeros(1,length(nprows));
        Solve_L_NEWEST=zeros(1,length(nprows));
        Solve_L_NEWEST_minmax=zeros(2,length(nprows));
        Solve_L_NEWEST_OLD=zeros(1,length(nprows));
        Solve_L_NEWEST_OLD=zeros(1,length(nprows));

        Solve_U_OLD=zeros(1,length(nprows));
        Solve_U_OLD_minmax=zeros(2,length(nprows));
        Solve_U_COMP_OLD=zeros(1,length(nprows));
        Solve_U_COMM_OLD=zeros(1,length(nprows));        
        Solve_U_NEWEST=zeros(1,length(nprows));
        Solve_U_NEWEST_minmax=zeros(2,length(nprows));
        Solve_U_COMP_NEWEST=zeros(1,length(nprows));
        Solve_U_COMM_NEWEST=zeros(1,length(nprows));          
        Zcomm_OLD=zeros(1,length(nprows));
        Zcomm_OLD_minmax=zeros(2,length(nprows));
        Zcomm_NEWEST=zeros(1,length(nprows));
        Zcomm_NEWEST_minmax=zeros(2,length(nprows));
    
        for npp=1:length(nprows)
       
            ncol=npcols(npp);
            nrow=nprows(npp);    



            filename = ['./',code,'/build/',mat,'/SLU.o_mpi_',num2str(nrow),'x',num2str(ncol),'x',num2str(npz),'_1_3d_old_nrhs_',num2str(nrhs)];
            fid = fopen(filename);
            while(~feof(fid))
                str=fscanf(fid,'%s',1);
            
                if(strcmp(str,'|forwardSolve'))
                    str=fscanf(fid,'%s',1);
                    str=fscanf(fid,'%f',1);
                    Solve_L_OLD(npp)=str;
                    str=fscanf(fid,'%s',1);
                    str=fscanf(fid,'%f',1);                    
                    Solve_L_OLD_minmax(1,npp)=str;
                    str=fscanf(fid,'%s',1);
                    str=fscanf(fid,'%f',1);                    
                    Solve_L_OLD_minmax(2,npp)=str;   
                end

                if(strcmp(str,'|forwardSolve-compute'))
                    str=fscanf(fid,'%s',1);
                    str=fscanf(fid,'%f',1);
                    Solve_L_COMP_OLD(npp)=str;
                end

                if(strcmp(str,'|forwardSolve-comm'))
                    str=fscanf(fid,'%s',1);
                    str=fscanf(fid,'%f',1);
                    Solve_L_COMM_OLD(npp)=str;
                end                

                if(strcmp(str,'|backSolve'))
                    str=fscanf(fid,'%s',1);
                    str=fscanf(fid,'%f',1);
                    Solve_U_OLD(npp)=str;
                    str=fscanf(fid,'%s',1);
                    str=fscanf(fid,'%f',1);                    
                    Solve_U_OLD_minmax(1,npp)=str;
                    str=fscanf(fid,'%s',1);
                    str=fscanf(fid,'%f',1);                    
                    Solve_U_OLD_minmax(2,npp)=str;   
                end
    
                if(strcmp(str,'|backSolve-compute'))
                    str=fscanf(fid,'%s',1);
                    str=fscanf(fid,'%f',1);
                    Solve_U_COMP_OLD(npp)=str;
                end

                if(strcmp(str,'|backSolve-comm'))
                    str=fscanf(fid,'%s',1);
                    str=fscanf(fid,'%f',1);
                    Solve_U_COMM_OLD(npp)=str;
                end                


                if(strcmp(str,'|trs_comm_z'))
                    str=fscanf(fid,'%s',1);
                    str=fscanf(fid,'%f',1);
                    Zcomm_OLD(npp)=str;
                    str=fscanf(fid,'%s',1);
                    str=fscanf(fid,'%f',1);
                    Zcomm_OLD_minmax(1,npp)=str;      
                    str=fscanf(fid,'%s',1);
                    str=fscanf(fid,'%f',1);
                    Zcomm_OLD_minmax(2,npp)=str;                          
                end
    
                if(strcmp(str,'SOLVE'))
                   str=fscanf(fid,'%s',1);
                   if(strcmp(str,'time'))
                       str=fscanf(fid,'%f',1);
                       Solve_SLU_OLD(npp)=str;
                   end
                end    
                if(strcmp(str,'Solve'))
                   str=fscanf(fid,'%s',1);
                   if(strcmp(str,'flops'))
                       str=fscanf(fid,'%f',1);
                       Flops_SLU_OLD(npp)=str;
                   end
                end  
            end 
            fclose(fid);



            filename = ['./',code,'/build/',mat,'/SLU.o_mpi_',num2str(nrow),'x',num2str(ncol),'x',num2str(npz),'_1_3d_newest_nrhs_',num2str(nrhs)];
            fid = fopen(filename);
            while(~feof(fid))
                str=fscanf(fid,'%s',1);
            
                if(strcmp(str,'|forwardSolve'))
                    str=fscanf(fid,'%s',1);
                    str=fscanf(fid,'%f',1);
                    Solve_L_NEWEST(npp)=str;
                    str=fscanf(fid,'%s',1);
                    str=fscanf(fid,'%f',1);                    
                    Solve_L_NEWEST_minmax(1,npp)=str;
                    str=fscanf(fid,'%s',1);
                    str=fscanf(fid,'%f',1);                    
                    Solve_L_NEWEST_minmax(2,npp)=str;                    
                end
    

                if(strcmp(str,'|forwardSolve-compute'))
                    str=fscanf(fid,'%s',1);
                    str=fscanf(fid,'%f',1);
                    Solve_L_COMP_NEWEST(npp)=str;
                end

                if(strcmp(str,'|forwardSolve-comm'))
                    str=fscanf(fid,'%s',1);
                    str=fscanf(fid,'%f',1);
                    Solve_L_COMM_NEWEST(npp)=str;
                end                


                if(strcmp(str,'|backSolve'))
                    str=fscanf(fid,'%s',1);
                    str=fscanf(fid,'%f',1);
                    Solve_U_NEWEST(npp)=str;
                    str=fscanf(fid,'%s',1);
                    str=fscanf(fid,'%f',1);                    
                    Solve_U_NEWEST_minmax(1,npp)=str;
                    str=fscanf(fid,'%s',1);
                    str=fscanf(fid,'%f',1);                    
                    Solve_U_NEWEST_minmax(2,npp)=str;                           
                end
    
    
                if(strcmp(str,'|backSolve-compute'))
                    str=fscanf(fid,'%s',1);
                    str=fscanf(fid,'%f',1);
                    Solve_U_COMP_NEWEST(npp)=str;
                end

                if(strcmp(str,'|backSolve-comm'))
                    str=fscanf(fid,'%s',1);
                    str=fscanf(fid,'%f',1);
                    Solve_U_COMM_NEWEST(npp)=str;
                end     

                if(strcmp(str,'|trs_comm_z'))
                    str=fscanf(fid,'%s',1);
                    str=fscanf(fid,'%f',1);
                    Zcomm_NEWEST(npp)=str;
                    str=fscanf(fid,'%s',1);
                    str=fscanf(fid,'%f',1);
                    Zcomm_NEWEST_minmax(1,npp)=str;      
                    str=fscanf(fid,'%s',1);
                    str=fscanf(fid,'%f',1);
                    Zcomm_NEWEST_minmax(2,npp)=str;                        
                end
    
                if(strcmp(str,'SOLVE'))
                   str=fscanf(fid,'%s',1);
                   if(strcmp(str,'time'))
                       str=fscanf(fid,'%f',1);
                       Solve_SLU_NEWEST(npp)=str;
                   end
                end    
                if(strcmp(str,'Solve'))
                   str=fscanf(fid,'%s',1);
                   if(strcmp(str,'flops'))
                       str=fscanf(fid,'%f',1);
                       Flops_SLU_NEWEST(npp)=str;
                   end
                end  
            end 
            fclose(fid);
    
    
        end
    
    
        SOLVE_SLU(zz,:,1)=Solve_SLU_OLD;
        SOLVE_SLU(zz,:,2)=Solve_SLU_NEWEST;
        SOLVE_SLU(zz,:,3)=npz.*npcols.*nprows;

        SOLVE_COMM_Z_OLD(zz,:)=Zcomm_OLD;
        SOLVE_COMP_2D_OLD(zz,:)=Solve_L_COMP_OLD+Solve_U_COMP_OLD;
        SOLVE_COMM_2D_OLD(zz,:)=Solve_L_COMM_OLD+Solve_U_COMM_OLD;
        SOLVE_L_Z_OLD_minmax(zz,:,1)=Solve_L_OLD;
        SOLVE_L_Z_OLD_minmax(zz,:,2)=Solve_L_OLD_minmax(1,:);
        SOLVE_L_Z_OLD_minmax(zz,:,3)=Solve_L_OLD_minmax(2,:);
        SOLVE_U_Z_OLD_minmax(zz,:,1)=Solve_U_OLD;
        SOLVE_U_Z_OLD_minmax(zz,:,2)=Solve_U_OLD_minmax(1,:);
        SOLVE_U_Z_OLD_minmax(zz,:,3)=Solve_U_OLD_minmax(2,:);
        SOLVE_COMM_Z_OLD_minmax(zz,:,1)=Zcomm_OLD;
        SOLVE_COMM_Z_OLD_minmax(zz,:,2)=Zcomm_OLD_minmax(1,:);
        SOLVE_COMM_Z_OLD_minmax(zz,:,3)=Zcomm_OLD_minmax(2,:);

        SOLVE_COMM_Z_NEWEST(zz,:)=Zcomm_NEWEST;
        SOLVE_COMP_2D_NEWEST(zz,:)=Solve_L_COMP_OLD+Solve_U_COMP_NEWEST;
        SOLVE_COMM_2D_NEWEST(zz,:)=Solve_L_COMM_OLD+Solve_U_COMM_NEWEST;


        SOLVE_L_Z_NEWEST_minmax(zz,:,1)=Solve_L_NEWEST;
        SOLVE_L_Z_NEWEST_minmax(zz,:,2)=Solve_L_NEWEST_minmax(1,:);
        SOLVE_L_Z_NEWEST_minmax(zz,:,3)=Solve_L_NEWEST_minmax(2,:);
        SOLVE_U_Z_NEWEST_minmax(zz,:,1)=Solve_U_NEWEST;
        SOLVE_U_Z_NEWEST_minmax(zz,:,2)=Solve_U_NEWEST_minmax(1,:);
        SOLVE_U_Z_NEWEST_minmax(zz,:,3)=Solve_U_NEWEST_minmax(2,:);
        SOLVE_COMM_Z_NEWEST_minmax(zz,:,1)=Zcomm_NEWEST;
        SOLVE_COMM_Z_NEWEST_minmax(zz,:,2)=Zcomm_NEWEST_minmax(1,:);
        SOLVE_COMM_Z_NEWEST_minmax(zz,:,3)=Zcomm_NEWEST_minmax(2,:);


    end


    figure(1)
    
    origin = [200,60];
    fontsize = 32;
    axisticksize = 32;
    markersize = 10;
    LineWidth = 3;
    colormap(cmap);
    y = [SOLVE_L_Z_OLD_minmax(:,:,1), SOLVE_L_Z_NEWEST_minmax(:,:,1)];
    errY = zeros(size(y,1),size(y,2),2);
    errY(:,1,1) = SOLVE_L_Z_OLD_minmax(:,:,1) - SOLVE_L_Z_OLD_minmax(:,:,2);
    errY(:,2,1) = SOLVE_L_Z_NEWEST_minmax(:,:,1) - SOLVE_L_Z_NEWEST_minmax(:,:,2);
    errY(:,1,2) = SOLVE_L_Z_OLD_minmax(:,:,3) - SOLVE_L_Z_OLD_minmax(:,:,1);
    errY(:,2,2) = SOLVE_L_Z_NEWEST_minmax(:,:,3) - SOLVE_L_Z_NEWEST_minmax(:,:,1);


    [hBar, hErrorbar] = barwitherr(errY, y); % Plot with errorbars
    set(hErrorbar(:), 'LineWidth', 2)

    gca = get(gcf,'CurrentAxes');
     

    Xlabels={};
    for ii=1:length(npzs)
        Xlabels{ii}=num2str(npzs(ii));
    end
    set(gca,'XTick',1:1:length(npzs));
    xticklabels(Xlabels);
 
    P=npzs(1)*nprows_all(1)*npcols_all(1);

    title(['L Solve for $P$=',num2str(P)],'interpreter','latex');

    gca = get(gcf,'CurrentAxes');
    set( gca, 'FontName','Times New Roman','fontsize',axisticksize);
    str = sprintf('Time (s)');
    ylabel(str,'interpreter','latex')
    xlabel('$Pz$','interpreter','latex')
    
%     title([mats_nopost{nm},' nrhs=',num2str(nrhs)],'interpreter','none');

    legs={'baseline','proposed'}
     gca=legend(legs,'interpreter','latex','color','none','NumColumns',2);
    
    set(gcf,'Position',[origin,800,450]);
    
    fig = gcf;
%     style = hgexport('factorystyle');
%     style.Bounds = 'tight';
    % hgexport(fig,'-clipboard',style,'applystyle', true);
    
    
    str = ['Loadbalance_L',mats_nopost{nm},'_nrhs_',num2str(nrhs),'_P_',num2str(P),'.eps'];
    saveas(fig,str,'epsc')


    figure(2)
    
    origin = [200,60];
    fontsize = 32;
    axisticksize = 32;
    markersize = 10;
    LineWidth = 3;
    colormap(cmap);
    y = [SOLVE_U_Z_OLD_minmax(:,:,1), SOLVE_U_Z_NEWEST_minmax(:,:,1)];
    errY = zeros(size(y,1),size(y,2),2);
    errY(:,1,1) = SOLVE_U_Z_OLD_minmax(:,:,1) - SOLVE_U_Z_OLD_minmax(:,:,2);
    errY(:,2,1) = SOLVE_U_Z_NEWEST_minmax(:,:,1) - SOLVE_U_Z_NEWEST_minmax(:,:,2);
    errY(:,1,2) = SOLVE_U_Z_OLD_minmax(:,:,3) - SOLVE_U_Z_OLD_minmax(:,:,1);
    errY(:,2,2) = SOLVE_U_Z_NEWEST_minmax(:,:,3) - SOLVE_U_Z_NEWEST_minmax(:,:,1);


    [hBar, hErrorbar] = barwitherr(errY, y); % Plot with errorbars
    set(hErrorbar(:), 'LineWidth', 2)

    gca = get(gcf,'CurrentAxes');
     

    Xlabels={};
    for ii=1:length(npzs)
        Xlabels{ii}=num2str(npzs(ii));
    end
    set(gca,'XTick',1:1:length(npzs));
    xticklabels(Xlabels);
 
    P=npzs(1)*nprows_all(1)*npcols_all(1);

    title(['U Solve for $P$=',num2str(P)],'interpreter','latex');

    gca = get(gcf,'CurrentAxes');
    set( gca, 'FontName','Times New Roman','fontsize',axisticksize);
    str = sprintf('Time (s)');
    ylabel(str,'interpreter','latex')
    xlabel('$Pz$','interpreter','latex')
    
%     title([mats_nopost{nm},' nrhs=',num2str(nrhs)],'interpreter','none');


    legs={'baseline','proposed'}
     gca=legend(legs,'interpreter','latex','color','none','NumColumns',2);
    set(gcf,'Position',[origin,800,450]);
    
    fig = gcf;
%     style = hgexport('factorystyle');
%     style.Bounds = 'tight';
    % hgexport(fig,'-clipboard',style,'applystyle', true);
    
    
    str = ['Loadbalance_U',mats_nopost{nm},'_nrhs_',num2str(nrhs),'_P_',num2str(P),'.eps'];
    saveas(fig,str,'epsc')

end
