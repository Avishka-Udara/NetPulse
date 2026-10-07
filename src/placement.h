#ifndef NETPULSE_PLACEMENT_H
#define NETPULSE_PLACEMENT_H
typedef struct { int left,right; } Occupied;
/* Find the closest fitting gap, including margins already added to obstacles. */
static int placement_find(int left,int right,int width,int preferred,Occupied *items,int count,int *result) {
    for(int i=1;i<count;i++){Occupied v=items[i];int j=i;while(j>0 && items[j-1].left>v.left){items[j]=items[j-1];j--;}items[j]=v;}
    int cursor=left,found=0,best=0,distance=0;
    for(int i=0;i<=count;i++){
        int end=i<count?items[i].left:right;if(end>right)end=right;
        if(end-cursor>=width){int x=preferred;if(x<cursor)x=cursor;if(x>end-width)x=end-width;int d=x>preferred?x-preferred:preferred-x;
            if(!found || d<distance){found=1;best=x;distance=d;}}
        if(i<count && items[i].right>cursor)cursor=items[i].right;
        if(cursor>right)break;
    }
    if(found)*result=best;return found;
}
#endif
