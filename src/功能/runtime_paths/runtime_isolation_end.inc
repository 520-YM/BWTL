static void runtime_isolation_end(void){if(runtime_data_mutex){ReleaseMutex(runtime_data_mutex);CloseHandle(runtime_data_mutex);runtime_data_mutex=NULL;}}
