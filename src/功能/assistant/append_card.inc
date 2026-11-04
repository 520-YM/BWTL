static void append_card(const wchar_t *role,const wchar_t *text){
    wchar_t label[64];swprintf_s(label,64,L"\n【%ls】\n",role);
    store_tail_add(history,sizeof(history)/sizeof(history[0]),label);
    store_tail_add(history,sizeof(history)/sizeof(history[0]),text);
    store_tail_add(history,sizeof(history)/sizeof(history[0]),L"\n\n");
    trace("Update native chat cards",native_ui_history(history)?0:-1);
}
