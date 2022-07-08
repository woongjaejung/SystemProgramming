#include "csapp.h"
struct item stock_tree[MAXTREE+1];
int max_node=0;
sbuf_t sbuf;					//	worker thread pool shared buffer

/* function prototypes */
int find_node(int _id);			//	get index of stock in stock_tree
void sigint_handler(int sig);	//	signal handler for SIGINT
void save_stock(void);			//	update stock.txt

void init(){
    FILE* fp=fopen("stock.txt", "r");
    int id, amount, price;

    for(int i=0; i<=MAXTREE; i++)
        stock_tree[i].id=-1;	//	if binary tree is empty, id is -1

    while(fscanf(fp, "%d %d %d", &id, &amount, &price) != EOF){
		/* make binary search tree */
        int n=1;
        while(stock_tree[n].id != -1){
            if(id < stock_tree[n].id) n=2*n;
            else if (id > stock_tree[n].id) n=2*n+1;
            else{ 
                printf("SAME ID ERROR \n");
                break;
            }
        }
        if(n>max_node) max_node=n;
        stock_tree[n].id=id;
        stock_tree[n].left_stock=amount;
        stock_tree[n].price=price;
        stock_tree[n].readcnt=0;
        Sem_init(&stock_tree[n].mutex, 0, 1);
        Sem_init(&stock_tree[n].update, 0, 1);
    }
}

void show_tree(int n, char *buf){
    // use DFS

    if(stock_tree[n].id == -1 || n > max_node)	//	return condition
		return;

    char tmp_buf[30];
    memset(tmp_buf, 0, sizeof(tmp_buf));
    P(&stock_tree[n].mutex);				//	Readers favor
    if(++(stock_tree[n].readcnt) == 1){     /* First in */
        P(&stock_tree[n].update);           //  lock update; writers cannot access node
    }
    V(&stock_tree[n].mutex);

    /* Critical section for stock_tree[n] */
    sprintf(tmp_buf, "%d %d %d\n", stock_tree[n].id,
            stock_tree[n].left_stock, stock_tree[n].price); // get the status of stock
    strcat(buf, tmp_buf);					//	concatenate to user buffer
    /* Critical section end */

    P(&stock_tree[n].mutex);
    if(--(stock_tree[n].readcnt)==0){    /* Last out */
        V(&stock_tree[n].update);       //  unlock update
    //    printf("read %d V\n", n);
    }
    V(&stock_tree[n].mutex);

    show_tree(2*n, buf); 
    show_tree(2*n+1, buf);
}

void trade(int connfd){
    int n;
    char tmp1[10], tmp2[10], tmp3[10];
    char op[5];
    int _id, _amount;
    char buf[MAXLINE];		//	user buffer
    rio_t rio;				

//    memset(buf, 0, sizeof(buf));
    Rio_readinitb(&rio, connfd);
    while((n = Rio_readlineb(&rio, buf, MAXLINE)) != 0) {
        sscanf(buf, "%s", op);		//	get command

        if(!strcmp(op, "show")){
            show_tree(1, buf);
        }
        else if(!strcmp(op, "exit")){
            strcpy(buf, "exit\n");
        }
        else if(!strcmp(op, "buy")){ 
			// buy ID amount
			// cannot buy when short remains
            sscanf(buf, "%s %s %s", tmp1, tmp2, tmp3);
            _id=(int)atoi(tmp2);    _amount=(int)atoi(tmp3);
            _id=find_node(_id);

            P(&stock_tree[_id].update); 
            if(stock_tree[_id].left_stock >= _amount) {
                stock_tree[_id].left_stock-=_amount;
                strcpy(buf, "[buy] success\n");
            }
            else {
                strcpy(buf, "Not enough left stocks\n");
            }
            V(&stock_tree[_id].update); 
        }
        else if(!strcmp(op, "sell")){ 
			// sell ID amount
			// sell always success
            sscanf(buf, "%s %s %s", tmp1, tmp2, tmp3);
            _id=(int)atoi(tmp2);    _amount=(int)atoi(tmp3);
            _id=find_node(_id);

            P(&stock_tree[_id].update); 
            stock_tree[_id].left_stock+=_amount;
            strcpy(buf, "[sell] success\n");
            V(&stock_tree[_id].update); 
        }
        else {
			// exception never entered
            printf("CLIENT MSG ERROR\n");
            continue;
        }

		/* write corresponding msg to client */
    	Rio_writen(connfd, buf, MAXLINE);
    }

}

int main(int argc, char **argv) 
{
    int listenfd, connfd;
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;  	/* Enough space for any address */  //line:netp:echoserveri:sockaddrstorage
    pthread_t tid;

    if (argc != 2) {
	fprintf(stderr, "usage: %s <port>\n", argv[0]);
	exit(0);
    }

	signal(SIGINT, sigint_handler);			//	if CTRL+C entered server, server must save the stock info to memory
    listenfd = Open_listenfd(argv[1]);
    init();
	
	sbuf_init(&sbuf, SBUFSIZE);
   
	for(int i=0; i<NTHREADS; i++)
		Pthread_create(&tid, NULL, thread, NULL);					//	create worker threads

    while(1){
        clientlen=sizeof(struct sockaddr_storage);
        connfd=Accept(listenfd, (SA*) &clientaddr, &clientlen);		//	connection request from client
   		sbuf_insert(&sbuf, connfd); 								//	insert fd to sbuf
	}

    exit(0);
}

void *thread(void *vargp){
    Pthread_detach(pthread_self());		//	detach from master
   	while(1){
		int connfd=sbuf_remove(&sbuf);
		trade(connfd);					//	connected with client
		Close(connfd);

		if(sbuf_empty(&sbuf))			//	if no connection to client
			save_stock();				//	update stock.txt
	}

}

int find_node(int _id){
	/* find the index of stock with _id in stock_tree */
    int n=1;
    while(stock_tree[n].id != -1){
        if(stock_tree[n].id == _id) return n;
        else if(stock_tree[n].id < _id) n=2*n+1;
        else if(stock_tree[n].id > _id) n=2*n;
    }
    return -1;
}

void save_stock(){
	FILE* fp = fopen("stock.txt", "w");
	char buf[MAXLINE];
	memset(buf, 0, sizeof(buf));
	show_tree(1, buf);
	Fputs(buf, fp);                     //  update stock.txt
	Fclose(fp);
}

void sigint_handler(int sig){
	save_stock();						//	if server end
	exit(0);							//	update stock.txt
}
