static bool greater(void *a, void *b) { return *(int *)a > *(int *)b; }
static void test_paths(void) {
    char path[255], ext[8];
    assert(General_CreateFilePathFromFolderAndFile(path,"1:","test") && !strcmp(path,"1:test"));
    assert(General_CreateFilePathFromFolderAndFile(path,"2:dir","test") && !strcmp(path,"2:dir/test"));
    assert(General_CreateFilePathFromFolderAndFile(path,"0:a/b","..") && !strcmp(path,"0:a"));
    assert(General_CreateFilePathFromFolderAndFile(path,"0:a","..") && !strcmp(path,"0:"));
    assert(General_CreateFilePathFromFolderAndFile(path,"3:","..") && !strcmp(path,"3:"));
    assert(General_CreateFilePathFromFolderAndFile(path,"0:a/b/","..") && !strcmp(path,"0:a"));
    assert(!General_CreateFilePathFromFolderAndFile(path,"0:","1:bad"));
    assert(!General_CreateFilePathFromFolderAndFile(path,"0:","dir/file"));
    memset(path,'x',254); path[254]=0;
    assert(!General_CreateFilePathFromFolderAndFile(path,path,"extra"));
    assert(General_ExtractFileExtensionFromFilename("a.PGZ",ext) && !strcmp(ext,"pgz"));
    assert(!General_ExtractFileExtensionFromFilename("a.123456789012345",ext));
    assert(!General_ExtractFileExtensionFromFilename("a.",ext));
    { char tiny[2]; assert(General_Strlcpy(tiny,"abc",2)==2 && !strcmp(tiny,"a")); }
}
static void test_sort(void) {
    WB2KList nodes[255], *head=NULL, *last=NULL; int values[255]; int i;
    List_MergeSort(&head,greater); List_RepairPrevLinks(&head);
    for(i=0;i<255;++i) { values[i]=(i*37)%255; nodes[i].payload_=&values[i]; nodes[i].next_item_=head; nodes[i].prev_item_=&nodes[i]; head=&nodes[i]; }
    List_MergeSort(&head,greater); List_RepairPrevLinks(&head);
    for(i=0;i<255;++i) { assert(head && *(int *)head->payload_==i); assert(head->prev_item_==last); last=head; head=head->next_item_; }
    assert(!head);
    head=&nodes[0]; head->next_item_=NULL; head->prev_item_=head; List_RepairPrevLinks(&head); assert(!head->prev_item_);
}
static void test_hex(void) {
    char s[80], *ptr=s;
    strcpy(s,"#00,ffA1"); assert(ScreenEvaluateUserStringForHexSeries(&ptr)==3 && !memcmp(s,"\0\xff\xa1",3));
    strcpy(s,"#1"); assert(!ScreenEvaluateUserStringForHexSeries(&ptr));
    strcpy(s,"#GG"); assert(!ScreenEvaluateUserStringForHexSeries(&ptr));
    strcpy(s,"#12,"); assert(!ScreenEvaluateUserStringForHexSeries(&ptr));
    strcpy(s,"#12,,34"); assert(!ScreenEvaluateUserStringForHexSeries(&ptr));
    strcpy(s,"hello"); assert(ScreenEvaluateUserStringForHexSeries(&ptr)==5);
}
static void test_copy(void) {
    size_t sizes[]={0,1,255,256,257,1025}; size_t i,j;
    for(i=0;i<sizeof(input);++i) input[i]=(unsigned char)i;
    for(i=0;i<6;++i) {
        fault=0; input_size=sizes[i]; input_pos=output_size=0; close_count=0;
        assert(Folder_CopyFileBytes("source","target",0)==(int32_t)input_size);
        assert(output_size==input_size && !memcmp(input,output,input_size));
        assert(close_count==2 && shown==hidden);
    }
    for(j=1;j<=5;++j) {
        fault=j; input_size=257; input_pos=output_size=0; close_count=0;
        assert(Folder_CopyFileBytes("source","target",257)==-1);
        assert(close_count==(j==1?0:j==2?1:2)); assert(shown==hidden);
    }
    assert(Folder_CopyFileBytes("same","same",0)==-1);
}
static void test_messages(void) {
    char long_name[1001]; memset(long_name,'x',1000); long_name[1000]=0;
    refreshes=0; Buffer_NewMessage(long_name); assert(refreshes==13);
    assert(strlen(row[2])==78 && row[2][76]==' ');
    drawn[0]=0;
    assert(!EM_WrapAndDisplayString(long_name,0,0,77,14));
    assert(strlen(drawn)==1000 && !strcmp(drawn,long_name));
    strcpy(long_name,"a\r\nb\rc\nd"); drawn[0]=0;
    assert(!EM_WrapAndDisplayString(long_name,0,0,77,4) && !strcmp(drawn,"abcd"));
}
static void test_editor(void) {
    char s[5]="abcd"; keys=(const unsigned char *)"z\r"; key_index=0; drawn[0]=0;
    assert(Text_GetStringFromUser(s,4,1,1,false) && !strcmp(s,"abcd") && !cursor_visible);
    keys=(const unsigned char *)"\x10\x7fZ\r"; key_index=0;
    assert(Text_GetStringFromUser(s,4,1,1,false) && !strcmp(s,"Zbcd"));
    keys=(const unsigned char *)"\x08\x08\r"; key_index=0;
    assert(Text_GetStringFromUser(s,4,1,1,false) && !strcmp(s,"Zb"));
    keys=(const unsigned char *)"\x1b"; key_index=0;
    assert(!Text_GetStringFromUser(s,4,1,1,false) && !cursor_visible);
}
int main(void) { test_paths(); test_sort(); test_hex(); test_copy(); test_messages(); test_editor(); puts("Host regressions passed"); return 0; }
