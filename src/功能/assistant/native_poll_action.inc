static void native_poll_action(void) { PostMessageW(native_backend_window,WM_APP+44,0,0); }

static void native_cancel_action(void){PostMessageW(native_backend_window,WM_APP+45,0,0);}
static void native_apply_action(void){PostMessageW(native_backend_window,WM_APP+46,0,0);}
static void native_details_action(void){PostMessageW(native_backend_window,WM_APP+48,0,0);}
