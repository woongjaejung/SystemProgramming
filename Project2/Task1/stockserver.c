#include "csapp.h"
struct item stock_tree[MAXTREE+1];
int max_node;
int client_cnt;						//	if no connected client, update stock.txt

/* function prototypes */
int find_node(int _id);				//	get index of stock in stock_tree
void sigint_handler(int sig);		//	signal handler for SIGINT
void save_stock(void);				//	update stock.txt

void init(int listenfd, pool *p){
    FILE* fp=fopen("stock.txt", "r");
    int id, amount, price;
    client_cnt=0;
    /* initialize pool */
    p->maxi=-1;
    for(int i=0; i<MAXTREE; i++)
        p->clientfd[i]=-1;
    p->maxfd=listenfd;  //  initially, listenfd is only member of select read set
    FD_ZERO(&p->read_set);
    FD_SET(listenfd, &p->read_set);

    /* initialize stock_tree */
    for(int i=0; i<=MAXTREE; i++)
        stock_tree[i].id=-1;

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
//      stock_tree[n].readcnt=0;                // not used in this task
//      Sem_init(&stock_tree[n].mutex, 0, 1);   // not used in this task
//      Sem_init(&stock_tree[n].update, 0, 1);  // not used in this task
    }
}
void show_tree(int n, char *buf){
    // use DFS

    if(stock_tree[n].id == -1 || n > max_node) return;

    char tmp_buf[30];
    memset(tmp_buf, 0, sizeof(tmp_buf));

    sprintf(tmp_buf, "%d %d %d\n", stock_tree[n].id,
            stock_tree[n].left_stock, stock_tree[n].price); // change to Rio_writen
    strcat(buf, tmp_buf);

    show_tree(2*n, buf); 
    show_tree(2*n+1, buf);
}

void add_client(int connfd, pool *p){
	/* client fd is stored in p->clientfd[].
	 * we find the first empty array element of clientfd
	 * so, the last added fd(p->maxfd) is not always at the last of the array
	 * therefore we should maintain the maximum index; p->maxi
	 */
    int i;
    p->nready--;
    client_cnt++;
    for(i=0; i<MAXTREE; i++)
        if(p->clientfd[i]<0){
            /* Add connected descriptor to the pool */
            p->clientfd[i]=connfd;
            Rio_readinitb(&p->clientrio[i], connfd);

            /* Add the descriptor to descriptor set */
            FD_SET(connfd, &p->read_set);

            /* Update max descriptor and pool high water mark */
            if(connfd > p->maxfd)
                p->maxfd=connfd;		//	maximum fd
            if(i > p->maxi)
                p->maxi=i;				//	maximum index
            break;
        }
    if(i==MAXTREE) // No empty slot
        app_error("add_client error: Too many clients");
}
void trade(pool *p){
    int connfd;
    int n;
    char tmp1[10], tmp2[10], tmp3[10];
    char op[5];
    int _id, _amount;
    char buf[MAXLINE];
    rio_t rio;

//	memset(buf, 0, MAXLINE);

    for(int i=0; (i<=p->maxi) && (p->nready>0); i++){
        connfd=p->clientfd[i];
        rio=p->clientrio[i];

        if((connfd>0) && (FD_ISSET(connfd, &p->ready_set))){
			/* active descriptor */
            p->nready--;
            if((n=Rio_readlineb(&rio, buf, MAXLINE)) != 0){
                sscanf(buf, "%s", op);
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

                    if(stock_tree[_id].left_stock >= _amount) {
                        stock_tree[_id].left_stock-=_amount;
                        strcpy(buf, "[buy] success\n");
					
                    }
                    else {
                        strcpy(buf, "Not enough left stocks\n");
                    }
                }
                else if(!strcmp(op, "sell")){ 
					// sell ID amount
                    // sell always success
					sscanf(buf, "%s %s %s", tmp1, tmp2, tmp3);
                    _id=(int)atoi(tmp2);    _amount=(int)atoi(tmp3);
                    _id=find_node(_id);

                    stock_tree[_id].left_stock+=_amount;
                    strcpy(buf, "[sell] success\n");
                }
                else {
					// exception never entered
                    printf("CLIENT MSG ERROR\n");
                    continue;
                }
				
				/* write corresponding msg to client */
				Rio_writen(connfd, buf, MAXLINE);
			}
            else{	
				//	EOF entered
                Close(connfd);
                FD_CLR(connfd, &p->read_set);
                p->clientfd[i]=-1;

                client_cnt--;
                if(client_cnt==0){
					/* if no connected client, update stock.txt */
                   save_stock();
                }
            }
        }
    }
}


int main(int argc, char **argv) 
{
    int listenfd, connfd;
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;  /* Enough space for any address */  //line:netp:echoserveri:sockaddrstorage
    static pool pool;    

    if (argc != 2) {
	fprintf(stderr, "usage: %s <port>\n", argv[0]);
	exit(0);
    }
    
	signal(SIGINT, sigint_handler);				//	if CTRL+C entered server, server must save the stock info to memory
    listenfd = Open_listenfd(argv[1]);
    init(listenfd, &pool);

    while(1){
        pool.ready_set=pool.read_set;
        pool.nready=Select(pool.maxfd+1, &pool.ready_set, NULL, NULL, NULL);
       
        if(FD_ISSET(listenfd, &pool.ready_set)){
            clientlen=sizeof(struct sockaddr_storage);
            connfd=Accept(listenfd, (SA*)&clientaddr, &clientlen);
            add_client(connfd, &pool);
        }
        trade(&pool);

        
    }
    
    exit(0);
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
	/* update stock.txt */
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
