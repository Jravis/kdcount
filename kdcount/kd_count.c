#include "kdtree.h"

typedef struct KDCountData {
    KDAttr *attrs[2];
    int nedges;
    double * edges;
    uint64_t * count;
    double * weight;

    int Nw;
} KDCountData;

static inline int 
bisect_left(double key, double * r2, int N) 
{
    int left = 0, right = N;
    if(N == 0) return 0;
    if(key < r2[0]) return 0;
    if(key > r2[N-1]) return N;
    while(right > left) {
        int mid = left + ((right - left) >> 1);
        if(key > r2[mid]) {
            left = mid + 1;
        } else {
            right = mid;
        }
    }
    return left;
}
#if 0
This is not used.
static int bisect_right(double key, double * r2, int N) {
    if(key <= r2[0]) return 0;
    if(key > r2[N-1]) return N;
    int left = 0, right = N;
    while(right > left) {
        int mid = left + ((right - left) >> 1);
        if(key >= r2[mid]) {
            left = mid + 1;
        } else {
            right = mid;
        }
    }
    return left;
}
#endif
static void 
kd_count_check(KDCountData * kdcd, KDNode * nodes[2], 
        int start, int end) 
{

    ptrdiff_t i, j;
    int d;
    KDTree * t0 = nodes[0]->tree;
    KDTree * t1 = nodes[1]->tree;
    int Nd = t0->input.dims[1];
    int Nw = kdcd->Nw;

    double * p0base = alloca(nodes[0]->size * sizeof(double) * Nd);
    double * p1base = alloca(nodes[1]->size * sizeof(double) * Nd);
    double * w0base = alloca(nodes[0]->size * sizeof(double) * Nw);
    double * w1base = alloca(nodes[1]->size * sizeof(double) * Nw);
    /* collect all nodes[1] positions to a continue block */
    double * p1, * p0, *w0, *w1;
    double half[Nd];
    double full[Nd];

    if(t0->boxsize) {
        for(d = 0; d < Nd; d++) {
            half[d] = t0->boxsize[d] * 0.5;
            full[d] = t0->boxsize[d];
        }
    }

    kd_collect(nodes[0], &t0->input, p0base);
    kd_collect(nodes[1], &t1->input, p1base);
    if(Nw > 0) {
        kd_collect(nodes[0], &kdcd->attrs[0]->input, w0base);
        kd_collect(nodes[1], &kdcd->attrs[1]->input, w1base);
    }
    for (p0 = p0base, w0 = w0base, i = 0; i < nodes[0]->size; i++) {
        for (p1 = p1base, w1 = w1base, j = 0; j < nodes[1]->size; j++) {
            double rr = 0.0;
            for (d = 0; d < Nd; d++){
                double dx = p1[d] - p0[d];
                if (dx < 0) dx = - dx;
                if (t0->boxsize) {
                    if (dx > half[d]) dx = full[d] - dx;
                }
                rr += dx * dx;
            }
            int b = bisect_left(rr, &kdcd->edges[start], end - start) + start;
            if (b < kdcd->nedges) {
                kdcd->count[b] += 1;
                for(d = 0; d < Nw; d++) {
                    kdcd->weight[b * Nw + d] += w0[d] * w1[d];
                }
            }
            w1 += Nw;
            p1 += Nd;
        }
        w0 += Nw;
        p0 += Nd;
    }
    
}


static void 
kd_count_traverse(KDCountData * kdcd, KDNode * nodes[2], 
        int start, int end) 
{
    int Nd = nodes[0]->tree->input.dims[1];
    int Nw = kdcd->Nw;
    double distmax = 0, distmin = 0;
    int d;
    double *min0 = kd_node_min(nodes[0]);
    double *min1 = kd_node_min(nodes[1]);
    double *max0 = kd_node_max(nodes[0]);
    double *max1 = kd_node_max(nodes[1]);
    for(d = 0; d < Nd; d++) {
        double min, max;
        double realmin, realmax;
        min = min0[d] - max1[d];
        max = max0[d] - min1[d];
        kd_realdiff(nodes[0]->tree, min, max, &realmin, &realmax, d);
        distmin += realmin * realmin;
        distmax += realmax * realmax;
    }

    start = bisect_left(distmin, &kdcd->edges[start], end - start) + start;
    end = bisect_left(distmax, &kdcd->edges[start], end - start) + start;
    if(start >= kdcd->nedges) {
        /* too far! skip */
        return;
    }
    if(start == end) {
        /* all bins are quickly counted no need to open*/
        kdcd->count[start] += nodes[0]->size * nodes[1]->size;
        if(Nw > 0) {
            double * w0 = kd_attr_get_node(kdcd->attrs[0], nodes[0]);
            double * w1 = kd_attr_get_node(kdcd->attrs[1], nodes[1]);
            for(d = 0; d < Nw; d++) {
                kdcd->weight[start * Nw + d] += w0[d] * w1[d]; 
            }
        }
        return;
    }

    /* nodes may intersect, open them */
    int open = nodes[0]->size < nodes[1]->size;
    if(nodes[open]->dim < 0) {
        open = (open == 0);
    }
    if(nodes[open]->dim < 0) {
        /* can't open the nodes, need to enumerate */
        kd_count_check(kdcd, nodes, start, end);
    } else {
        KDNode * save = nodes[open];
        nodes[open] = save->link[0];
        kd_count_traverse(kdcd, nodes, start, end);
        nodes[open] = save->link[1];
        kd_count_traverse(kdcd, nodes, start, end);
        nodes[open] = save;
    } 
}

void 
kd_count(KDNode * nodes[2], KDAttr * attrs[2], 
        double * edges, uint64_t * count, double * weight, 
        int nedges) 
{
    double * edges2 = alloca(sizeof(double) * nedges);
    int Nw;
    if (attrs[0]) 
        Nw = attrs[0]->input.dims[1];
    else
        Nw = 0;

    KDCountData kdcd = {
        .attrs = {attrs[0], attrs[1]},
        .nedges = nedges,
        .edges = edges2,
        .count = count,
        .weight = weight,
        .Nw = Nw,
    };

    int d;
    int i;
    for(i = 0; i < nedges; i ++) {
        if(edges[i] >= 0)
            edges2[i] = edges[i] * edges[i];
        else
            edges2[i] = i - nedges;
        count[i] = 0;
        if(Nw > 0) {
            for(d = 0; d < Nw; d++) {
                weight[i * Nw + d] = 0;
            }
        }
    }
    
    kd_count_traverse(&kdcd, nodes, 0, nedges);
}