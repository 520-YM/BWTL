
            length=sqrt((v[3]-v[0])*(v[3]-v[0])+(v[4]-v[1])*(v[4]-v[1])+(v[5]-v[2])*(v[5]-v[2]));if(length<0.01){e=PRO_TK_BAD_INPUTS;goto done;}
            FD_TRY("equation curve kind",model_element(tx.root,PRO_E_CURVE_TYPE,1,PRO_CURVE_TYPE_FROM_EQUATION,NULL));
            FD_TRY("curve csys",fd_ref_element(&tx,tx.root,PRO_E_CRV_FR_EQ_REF_CSYS,&ref[0]));
            FD_TRY("curve Cartesian",model_element(tx.root,PRO_E_CRV_FR_EQ_CSYS_TYPE,1,PRO_CRV_FR_EQ_CSYS_TYPE_CARTESIAN,NULL));
            FD_TRY("curve parameter start",model_element(tx.root,PRO_E_CRV_FR_EQ_PARAM_MIN,2,0,NULL));
            FD_TRY("curve parameter end",model_element(tx.root,PRO_E_CRV_FR_EQ_PARAM_MAX,2,1,NULL));
            swprintf_s(equation,1024,L"x=%.15g+(%.15g)*t\ny=%.15g+(%.15g)*t\nz=%.15g+(%.15g)*t",v[0]*scale,(v[3]-v[0])*scale,v[1]*scale,(v[4]-v[1])*scale,v[2]*scale,(v[5]-v[2])*scale);
            FD_TRY("curve equation element",model_element(tx.root,PRO_E_CRV_ENTER_EQUATION,0,0,&el));FD_TRY("curve equation",ProElementWstringSet(el,equation));
        
