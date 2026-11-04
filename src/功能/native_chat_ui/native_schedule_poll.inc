static int native_schedule_poll(void){return native_window && SetTimer(native_window,CHAT_POLL,100,NULL)!=0;}
