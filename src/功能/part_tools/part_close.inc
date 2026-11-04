static int part_close(double a,double b){return isfinite(a)&&isfinite(b)&&fabs(a-b)<=1e-7*fmax(1.0,fmax(fabs(a),fabs(b)));}
