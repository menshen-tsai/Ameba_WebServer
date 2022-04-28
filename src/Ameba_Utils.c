#include <wifi_conf.h>

uint32_t Ameba_getMAC() {
	char buf[6];
	int rc;
	rc = wifi_get_mac_address(buf);
	return (uint32_t)(buf[0]);
}

char* unconstchar(const char* s) {
    if(!s)
      return NULL;
    int i;
    char* res = NULL;
    res = (char*) malloc(strlen(s)+1);
    if(!res){
        fprintf(stderr, "Memory Allocation Failed! Exiting...\n");
        return NULL;
    } else{
        for (i = 0; s[i] != '\0'; i++) {
            res[i] = s[i];
        }
        res[i] = '\0';
//        return res;
    }
    return res;
}