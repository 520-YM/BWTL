
#define OR_ROWS 256
#define OR_COLUMNS 8
#define OR_CELL 256
#define OR_TOL 0.001

typedef struct { wchar_t value[OR_COLUMNS][OR_CELL]; unsigned line; } OrRow;
typedef struct { int size,columns; OrRow *rows; } OrTable;
typedef struct { FILE *file; wchar_t final[MAX_PATH],temp[MAX_PATH]; } OrOutput;

static int or_safe_id(const wchar_t *id){
    size_t n=wcslen(id);
    if(!n||n>40)return 0;
    for(;*id;id++)
        if(!ax_learning_word(*id)&&!(*id>=0x3400&&*id<=0x9fff))return 0;
    return 1;
}

static int or_number(const wchar_t *input,double *value){
    wchar_t *end;
    double n=wcstod(input,&end);
    if(input==end||!isfinite(n)||fabs(n)>10000000)return 0;
    while(iswspace(*end))end++;
    if(*end&&_wcsicmp(end,L"mm")&&wcscmp(end,L"毫米"))return 0;
    *value=n;
    return 1;
}

static int or_columns(wchar_t *line,OrRow *row,int count){
    line[wcscspn(line,L"\r\n")]=0;
    for(int i=0;i<count;i++){
        wchar_t *tab=wcschr(line,L'\t');
        if((i<count-1)!=!!tab)return 0;
        if(tab)*tab=0;
        size_t len=wcslen(line);
        if(!len||len>=OR_CELL)return 0;
        wcscpy_s(row->value[i],OR_CELL,line);
        if(tab)line=tab+1;
    }
    return 1;
}

static int or_table_load(OrTable *table,const wchar_t *path,int count,PartText *out){
    FILE *file=ax_learning_open(path);
    if(!file){
        part_text_add(out,L"无法读取输入文件 %ls。\n",path);
        return 0;
    }
    table->rows=calloc(OR_ROWS,sizeof(*table->rows));
    table->columns=count;
    int valid=table->rows!=NULL;
    unsigned line=0;
    wchar_t buffer[2112];
    while(valid&&fgetws(buffer,_countof(buffer),file)){
        line++;
        wchar_t *start=buffer[0]==0xfeff?buffer+1:buffer;
        if(!wcschr(start,L'\n')&&!feof(file)){valid=0;break;}
        if(*start==L'#'||*start==L'\r'||*start==L'\n'||!*start)continue;
        if(table->size==OR_ROWS||!or_columns(start,&table->rows[table->size],count)){valid=0;break;}
        table->rows[table->size++].line=line;
    }
    if(ferror(file))valid=0;
    fclose(file);
    if(valid&&table->size)return 1;
    part_text_add(out,L"第 %u 行附近无法读取。每行应有 %d 列，字段用 Tab 隔开，最多 %d 行。\n",line,count,OR_ROWS);
    return 0;
}

static void or_free(OrTable *table){free(table->rows);memset(table,0,sizeof(*table));}

static int or_unique_column(const OrTable *table,int column,PartText *out){
    for(int i=0;i<table->size;i++)for(int j=0;j<i;j++)
        if(!_wcsicmp(table->rows[i].value[column],table->rows[j].value[column])){
            part_text_add(out,L"第 %u 行与第 %u 行重复定义 %ls。\n",
                table->rows[j].line,table->rows[i].line,table->rows[i].value[column]);
            return 0;
        }
    return 1;
}

static int or_find(const OrTable *table,int col,const wchar_t *value){
    for(int i=0;i<table->size;i++)if(!_wcsicmp(value,table->rows[i].value[col]))return i;
    return -1;
}

static int or_folder(const wchar_t *order,wchar_t path[MAX_PATH],int create){
    wchar_t storage[MAX_PATH];
    if(!or_safe_id(order)||!storage_method_directory(storage,L"订单改型",create)||
       swprintf_s(path,MAX_PATH,L"%ls\\%ls",storage,order)<0)return 0;
    if(create&&!CreateDirectoryW(path,NULL)&&GetLastError()!=ERROR_ALREADY_EXISTS)return 0;
    DWORD attributes=GetFileAttributesW(path);
    return attributes!=INVALID_FILE_ATTRIBUTES&&(attributes&FILE_ATTRIBUTE_DIRECTORY)&&
        !(attributes&FILE_ATTRIBUTE_REPARSE_POINT)&&storage_output_directory(path);
}

static int or_path(const wchar_t *order,const wchar_t *leaf,wchar_t path[MAX_PATH],int create){
    wchar_t folder[MAX_PATH];
    return or_folder(order,folder,create)&&swprintf_s(path,MAX_PATH,L"%ls\\%ls",folder,leaf)>=0;
}

static int or_output_begin(OrOutput *out,const wchar_t *order,const wchar_t *leaf){
    memset(out,0,sizeof(*out));
    if(!or_path(order,leaf,out->final,1))return 0;
    out->file=ax_study_atomic(out->final,out->temp);
    return out->file!=NULL;
}

static int or_output_end(OrOutput *out,int good){
    if(!out->file)return 0;
    if(good)return ax_study_commit(out->file,out->temp,out->final);
    fclose(out->file);
    DeleteFileW(out->temp);
    return 0;
}
