#include <unistd.h>
#include <stdio.h>
#include <sys/mman.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include "bmalloc.h" 

bm_option bm_mode = BestFit ;
bm_header bm_list_head = {0, 0, 0x0 } ; // bm_list_head 를 처음에 초기화 한다.

int count;
int given;
int given_page;

// returns the size field value of a fitting block to accommodate s bytes.
int fitting (size_t s) {
    // s bytes를 log2를 취한뒤, 나온 결과를 올림한다. 그러면 필요한 block의 지수값이 나온다.
    given += (int) s;
    int fit_size = (int) round(log2(s));
    given_page += pow(2, fit_size);

    return fit_size;
}

// returns the header address of the suspected sibling block of h.
void * sibling (void * h) {
    bm_header_ptr header = (bm_header_ptr)h ; //
    int index = ((char *)h - (char *)header->next) / (1 << header->size) ; // header size의 지수 값
    if (index % 2 == 0) 
        return (char *)h + (1 << header->size) ;
    else 
        return (char *)h - (1 << header->size) ;
}

//allocates a buffer of s-bytes and returns its starting address.
void * bmalloc (size_t s) {
    //fit_size를 저장한다. s 에다가 bm_heade트 struct가 갖는 바이트 까지 함께 고려하여 fitting 한다.
    int fit_size = fitting(s + sizeof(bm_header));

    // fit_size 가 12보다 즉 4096 보다 큰 경우 NULL 출력 --> exception input에 추가해보자.
    if(fit_size > 12) {
        printf("Requested size is too large\n");
        return NULL;
    }

    // best_fit 을 처음에 초기화 한다. 이 때, bm_header_ptr 로 하자.
    bm_header_ptr best_fit = NULL;
    // Node 를 찾는 travler 를 초기화 한다.
    bm_header_ptr traveler = bm_list_head.next;

    while(traveler){
        // travler used가 null 이어야 한다.
        // 1차 관문 ) travler->size 가 fit_size보다 크거나 같아야 한다. ( 그래야 best fit을 찾을 수가 있다)
        if(!traveler->used && traveler->size >= fit_size){
            // 2차 관문 ) bm_mode가 Besfit 이고, best_fit이 아무것도 없는 경우 (시작하는 경우) 이거나 
            // travler size가 best_fit의 size보다 작다면 best_fit 이 travler가 된다. ( bestfit 은 fit_size 보단 크거나 같은 것 중에 가장 작은 것을 선택해야 하기 때문이다. )
            if(bm_mode == BestFit && (best_fit == NULL || traveler->size < best_fit->size) ) {
                best_fit = traveler ; // best_fit 에 현재 traveler가 들어간다.
            }
            // else가 아니라 else if 인 이유는 , else 이면 경우의 수가 더 많아 진다. 그래서 특정 조건에만 else if 문을 통과하도록 유도 
            else if(bm_mode == FirstFit){
                best_fit = traveler ; // traveler->size >= fit_size 인 것 중 가장 처음 찾은 node
                break ;  // FirstFit이면 traveler->size >= fit_size인 순간 바로 while 문 빠져나가기
            }
        }
        // traveler 를 다음 노드로 진행 이동시킨다.
        traveler = traveler -> next ; 
    }

    // for (bm_header *itr = bm_list_head.next; itr != NULL; itr = itr->next) {
    //     if (!itr->used && itr->size >= fit_size) {
    //         if (best_fit == NULL || itr->size < best_fit->size) {
    //             best_fit = itr;
    //         }
    //     }
    // }

    // while문을 다 통과하고 나서도, best_fit 이 NULL인 경우
    if (best_fit == NULL) {
        // mmap으로 빈 페이지 생성 4096 byte 
        void* address = mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

        count += 1;
        
        best_fit = (bm_header_ptr)address; // mmap으로 생성된 page의 빈 페이지 address 가 들어 간다.
        best_fit->size = 12; // best_fit의 size는 12 (4096 bytes) 가장 최댓값을 집어 넣는다.
        best_fit->next = bm_list_head.next; // best_fit->next에는 bm_list_head.next 를 먼저 집어 넣는다.
        bm_list_head.next = best_fit; // bm_list_head.next 에다가 best_fit 을 집어 넣는다.
    }

    // best_fit -> size 가 fit_size가 클 경우에
    while(best_fit->size > fit_size) { 
        int sibling_size = best_fit->size - 1;  
        bm_header_ptr split = (bm_header_ptr)sibling(best_fit);
        split->size = sibling_size;
        split->used = 0;
        split->next = best_fit->next;
        best_fit->next = split;
        best_fit->size = sibling_size;
    }
    
    // best_fit 이 사용되었다는 것을 알려주기 위해 1 로 바꾼다.
    best_fit->used = 1;

    // best_fit 이 어떻게 사용 되었든지 , best_fit 을 의 주소에서 bm_header의 size를 더한다.
    return  ((char*)best_fit) + sizeof(bm_header); // (char*)캐스팅하는 이유는 , pointer를 산술하기 위함이다.
}

// ?? 아직 모름 연구해야함. GPT made it
void bfree (void * p) {
    if (p == NULL) return;

    bm_header *hdr = (bm_header*)((char*)p - sizeof(bm_header));
    hdr->used = 0;

    while(1) {
    bm_header *sibling_hdr = (bm_header*)sibling(hdr);
    if(sibling_hdr->used || sibling_hdr->size != hdr->size)
        break;

    if(sibling_hdr < hdr) {
        sibling_hdr->next = hdr->next;
        sibling_hdr->size++;
        hdr = sibling_hdr;
    } else {
        bm_header *prev;
        for(prev = bm_list_head.next; prev->next != hdr; prev = prev->next);
        prev->next = hdr->next;
        hdr->size++;
    }
    //given_page -= pow(2, p->size);
}

}

// resize the allocated memory buffer into s bytes. As the result of this operation, the data may be immigrated to a different address, as like realloc possibly does.
void *brealloc(void *p, size_t s) {
    if (!p) {
        return bmalloc(s);
    }

    if (!s) {
        bfree(p);
        return NULL;
    }

bm_header_ptr header = (bm_header_ptr)(p - sizeof(bm_header));
size_t old_size = header->size - sizeof(bm_header);
size_t new_size = fitting(s + sizeof(bm_header));

if (old_size >= s) {
    if (old_size == new_size) {
        return p;
    }

    while (header->size > new_size) {
        int new_half_size = header->size / 2;
        bm_header_ptr buddy = (bm_header_ptr)sibling(header);
        buddy->used = 0;
        buddy->size = new_half_size;
        buddy->next = header->next;
        header->size = new_half_size;
        header->next = buddy;
    }
    return p;
} else {
        void *new_ptr = bmalloc(s);
        if (new_ptr) {
            memcpy(new_ptr, p, old_size);
            bfree(p);
        }
        return new_ptr;
    }
}

// set the space management scheme as BestFit, or FirstFit.
void bmconfig(bm_option opt) {
    bm_mode = opt;
}

// print out the internal status of the block list to the standard output.
void bmprint () 
{
	bm_header_ptr itr ;
	int i ;

	printf("==================== bm_list ====================\n") ;
	for (itr = bm_list_head.next, i = 0 ; itr != 0x0 ; itr = itr->next, i++) {
		printf("%3d:%p:%1d %8d:", i, ((void *) itr) + sizeof(bm_header), (int)itr->used, (int) itr->size) ;

		int j ;
		char * s = ((char *) itr) + sizeof(bm_header) ;
		for (j = 0 ; j < (itr->size >= 8 ? 8 : itr->size) ; j++) 
			printf("%02x ", s[j]) ;
		printf("\n") ;
	}
	printf("=================================================\n") ;

	//TODO: print out the stat's.

    //1) 만들어진 페이지가 총 몇 바이트인지
    printf("Total amount of all given memory: %d bytes\n", count*4096);
    //2) user가 요청한 메모리(free 안 한 것)
    printf("Total amount of memory given to the user: %d bytes\n", given);
    //3) 사용가능한 메모리 (남은 페이지)
    printf("Total amount of available memory: %d bytes\n", count*4096 - given_page - (int)sizeof(bm_header));
    //4) internal fragmentation payload 만
    printf("Total amount of the internal fragmentation: %d bytes\n", given_page - given - (int)sizeof(bm_header));
    printf("sizeof(bmheader) : %lu\n" , sizeof(bm_header));
}

/* 
0513
    - 코드 마무리. 및 주석 처리
    - bmprint 마무리
    - test2 , test3 분석하기

0514리
    - exception case 생각하기
    - Writing report
        - Explanations on the logics of important operations
        - Demonstration of multiple test scenarios that different aspects of your program work correctly

0515
    - 보고서 최종 마무리
        - Discussion on your results including challenges you have faced and/or questions you have bear in in doing this homework, new ideas for improvements, etc. You can achieve extra points if you write interesting discussions. 
            1.  sizeof(bm_header) ==> 9 byte라고 가정하고 했는데, 출력 해보니까 16이 나온다. why ? 왤까)?
             --> bm_header 구조체에는 다음의 멤버들이 있습니다:

                unsigned int used : 1; : 1 비트
                unsigned int size : 4; : 4 비트
                struct _bm_header * next; : 포인터 크기 (32비트 시스템에서 4바이트, 64비트 시스템에서 8바이트)
                그럼에도 불구하고, sizeof(bm_header)가 16바이트라고 나오는 이유는 아래와 같습니다:

                비트 필드: used와 size는 비트 필드로 선언되어 있습니다. 비트 필드는 여러 이유로 (예: 하드웨어 레지스터의 표현, 메모리 절약 등) 사용되지만, 한 가지 중요한 단점은 패딩 때문에 예상치 못한 공간을 차지할 수 있다는 것입니다. 비트 필드는 자신의 기본 형식에 따라 정렬됩니다. 여기서 used와 size는 unsigned int로 선언되었으므로, 이들은 unsigned int의 크기에 따라 정렬됩니다 (일반적으로 4바이트).

                포인터 크기: next 포인터의 크기는 컴파일러와 아키텍처에 따라 달라집니다. 32비트 컴파일러에서는 4바이트, 64비트 컴파일러에서는 8바이트를 차지합니다.

                구조체 정렬: 구조체는 가장 큰 자료형의 크기에 따라 정렬됩니다. 이 경우 next 포인터가 가장 큰 자료형이므로, 구조체는 next 포인터의 크기에 따라 정렬됩니다.

            2.

*/
