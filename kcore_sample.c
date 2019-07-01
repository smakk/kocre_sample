#include <elf.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>

struct kcore_list{
	struct kcore_list* list;
	unsigned long vaddr;
	unsigned long offset;
	size_t size;
};
struct kcore_list kcore;

/*
生成kcore链表,按照虚拟地址升序存放kcore提供的所有的地址空间,而且这些地址空间是不重叠的
*/
void* init_kcore_list(){
	int fd = open("/proc/kcore", O_RDONLY);
	if(fd < 0){
		printf("open /proc/kcore wrong\n");
		exit(-1);
	}
	Elf64_Ehdr head;
	read(fd,&head,sizeof(Elf64_Ehdr));
	if(lseek(fd,head.e_phoff,SEEK_SET)<0){
		printf("lseek /proc/kcore wrong\n");
		exit(-1);
	}
	Elf64_Phdr phdr[head.e_phnum];
	read(fd,&phdr,sizeof(Elf64_Phdr)*head.e_phnum);
	close(fd);
	int i;
	for(i=0;i<head.e_phnum;i++){
		struct kcore_list* k_list = malloc(sizeof(struct kcore_list));
		k_list->vaddr = phdr[i].p_vaddr;
		k_list->offset = head.e_phoff+sizeof(Elf64_Phdr)*i;
		k_list->size = phdr[i].p_memsz;
		struct kcore_list* tmplist = &kcore;
		while(tmplist->list != NULL && tmplist->list->vaddr<k_list->vaddr){
			tmplist = tmplist->list;
		}
		k_list->list = tmplist->list;
		tmplist->list = k_list;
		//printf("%lx\n",k_list->vaddr);
	}
	/*
	struct kcore_list* tmplist = kcore.list;
	unsigned long next = 0;
	while(tmplist != NULL){
		if(next>tmplist->vaddr + tmplist->size)
			printf("overlaped\n");
		printf("%lx\n",tmplist->vaddr);
		//printf("%lx\n",tmplist->size);
		//printf("%lx\n",next);
		next = tmplist->vaddr + tmplist->size;
		tmplist = tmplist->list;
	}
	*/
	return;
}

/*
根据虚拟地址addr,从/proc/kcore这个位置读取size大小的内存
*/
void* get_area(unsigned long addr, size_t size){
	void* ret;
	struct kcore_list* k_list = kcore.list;
	while(k_list != NULL && addr>k_list->vaddr + k_list->size){
		k_list = k_list->list;
	}
	/*
	printf("%lx\n", addr);
	printf("%lx\n", k_list->vaddr);
	printf("%ld\n", k_list->size);
	*/
	if(addr>=k_list->vaddr && addr+size<k_list->vaddr + k_list->size){
		int fd = open("/proc/kcore", O_RDONLY);
		if(fd < 0){
			printf("open /proc/kcore wrong\n");
			exit(-1);
		}
		if(lseek(fd,k_list->offset+(addr-k_list->vaddr),SEEK_SET)<0){
			printf("lseek /proc/kcore wrong\n");
			exit(-1);
		}
		ret = malloc(size);
		read(fd,ret,size);
		close(fd);
	}
	return ret;
}

//由于/proc/kcore没有节头，找不到符号表，所以通过kallsym来找符号地址
unsigned long get_sym(char* name){
	FILE *fd;
	char symbol_s[255];
	int i;
	unsigned long address;
	char tmp[255], type;

	if ((fd = fopen("/proc/kallsyms", "r")) == NULL)
	{
		printf("fopen /proc/kallsym wrong\n");
		exit(-1);
        }
	while(!feof(fd))
	{
		if(fscanf(fd, "%lx %c %s", &address, &type, symbol_s)<=0){
			printf("fscanf wrong\n");
			exit(-1);
		}
		if (strcmp(symbol_s, name) == 0)
		{
			fclose(fd);
			return address;
		}
		if (!strcmp(symbol_s, ""))
			break;
        }
	fclose(fd);
	return 0;
}

int main(){
	init_kcore_list();
	printf("_text addrs is %lx\n",get_sym("_text"));
	void * code = get_area(get_sym("init_task"),100);
	unsigned long * statue = (unsigned long *)code;
	//0代表正在运行,大于0表示停止了
	printf("init_task statue is %ld\n", *statue);
	return 0;
}
