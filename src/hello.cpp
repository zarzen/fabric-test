/**********************************************************************
 * 	Simple Hello Test
 * 	for
 * 	Open Fabric Interface 1.x
 *
 * 	Jianxin Xiong
 * 	(jianxin.xiong@intel.com)
 * 	2013-2017
 * ********************************************************************/
#include <iostream>
#include <thread>
#include <chrono>
#include <string>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <assert.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>
#include <unistd.h>
#include <rdma/fabric.h>
#include <rdma/fi_domain.h>
#include <rdma/fi_endpoint.h>
#include <rdma/fi_cm.h>
#include <rdma/fi_tagged.h>
#include <rdma/fi_rma.h>
#include <rdma/fi_atomic.h>
#include <rdma/fi_errno.h>

#define CHK_ERR(name, cond, err)							\
	do {										\
		if (cond) {								\
			fprintf(stderr,"%s: %s\n", name, strerror(-(err)));		\
			exit(1);							\
		}									\
	} while (0)

static struct fi_info		*fi;
static struct fid_fabric	*fabric;
static struct fid_domain	*domain;
static struct fid_av		*av;
static struct fid_ep		*ep;
static struct fid_cq		*cq;
static fi_addr_t		peer_addr;
static struct fi_context	sctxt;
static struct fi_context	rctxt;
static char			sbuf[64];
static char			rbuf[64];
int is_client = 0;
std::string nickname;
char* largebuff;
size_t largeSize = 2 * 1024 * 1024;

// static void get_peer_addr(void *peer_name)
// {
// 	int err;
// 	char buf[64];
// 	size_t len = 64;
    
// 	buf[0] = '\0';
// 	fi_av_straddr(av, peer_name, buf, &len);
// 	printf("Translating peer address: %s\n", buf);
//     // if (peer_addr==0ULL)
        
//     fi_addr_t pppt;
// 	err = fi_av_insert(av, peer_name, 1, &peer_addr, 0, NULL);
// 	CHK_ERR("fi_av_insert", (err!=1), err);
// }

static void init_fabric()
{
	struct fi_info		*hints;
	struct fi_cq_attr	cq_attr;
	struct fi_av_attr	av_attr;
	int 			err;
	int			version;
	char			name[64], buf[64];
	size_t			len = 64;

	hints = fi_allocinfo();
	CHK_ERR("fi_allocinfo", (!hints), -ENOMEM);

	memset(&cq_attr, 0, sizeof(cq_attr));
	memset(&av_attr, 0, sizeof(av_attr));
	memset(name, 0, len);
	memset(buf, 0, len);

	hints->ep_attr->type = FI_EP_RDM;
	// hints->caps = FI_MSG;
	// hints->mode = FI_CONTEXT;
	hints->fabric_attr->prov_name = strdup("efa");

	version = FI_VERSION(1, 9);
	err = fi_getinfo(version, NULL, NULL, 0, hints, &fi);
	CHK_ERR("fi_getinfo", (err<0), err);

	fi_freeinfo(hints);

	printf("Using OFI device: %s\n", fi->fabric_attr->name);

	err = fi_fabric(fi->fabric_attr, &fabric, NULL);
	CHK_ERR("fi_fabric", (err<0), err);

	err = fi_domain(fabric, fi, &domain, NULL);
	CHK_ERR("fi_domain", (err<0), err);

	av_attr.type = FI_AV_UNSPEC;

	err = fi_av_open(domain, &av_attr, &av, NULL);
	CHK_ERR("fi_av_open", (err<0), err);

	cq_attr.format = FI_CQ_FORMAT_TAGGED;
	cq_attr.size = 100;

	err = fi_cq_open(domain, &cq_attr, &cq, NULL);
	CHK_ERR("fi_cq_open", (err<0), err);

	err = fi_endpoint(domain, fi, &ep, NULL);
	CHK_ERR("fi_endpoint", (err<0), err);

	err = fi_ep_bind(ep, (fid_t)cq, FI_SEND|FI_RECV);
	CHK_ERR("fi_ep_bind cq", (err<0), err);

	err = fi_ep_bind(ep, (fid_t)av, 0);
	CHK_ERR("fi_ep_bind av", (err<0), err);

	err = fi_enable(ep);
	CHK_ERR("fi_enable", (err<0), err);

	err = fi_getname((fid_t)ep, name, &len);
	CHK_ERR("fi_getname", (err<0), err);

	buf[0] = '\0';
	len = 64;
	printf("My ep Name is buff contains \n");
	for (int i = 0; i < 64; i ++){
		printf("%d, ", name[i]);
	}
	printf("\n");
	fi_av_straddr(av, name, buf, &len);
	printf("My address is %s\n", buf);

}

int static finalize_fabric(void)
{
	fi_close((fid_t)ep);
	fi_close((fid_t)cq);
	fi_close((fid_t)av);
	fi_close((fid_t)domain);
	fi_close((fid_t)fabric);
	fi_freeinfo(fi);
}

static const char integ_alphabet[] = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
static const int integ_alphabet_length = (sizeof(integ_alphabet)/sizeof(*integ_alphabet)) - 1;
void ft_fill_buf(void *buf, int size)
{

	char *msg_buf;
	int msg_index;
	static unsigned int iter = 0;
	int i;

	msg_index = ((iter++)*7) % integ_alphabet_length;
	msg_buf = (char *)buf;
	for (i = 0; i < size; i++) {
		msg_buf[i] = integ_alphabet[msg_index++];
		if (msg_index >= integ_alphabet_length)
			msg_index = 0;
	}
}

static void wait_cq(void)
{
	struct fi_cq_err_entry entry;
	int ret, completed = 0;
	fi_addr_t from;

	while (!completed) {
		ret = fi_cq_readfrom(cq, &entry, 1, &from);
		if (ret == -FI_EAGAIN)
			continue;

		if (ret == -FI_EAVAIL) {
			ret = fi_cq_readerr(cq, &entry, 1);
			CHK_ERR("fi_cq_readerr", (ret!=1), ret);

			printf("Completion with error: %d\n", entry.err);
			// if (entry.err == FI_EADDRNOTAVAIL)
			// 	get_peer_addr(entry.err_data);
		}

		CHK_ERR("fi_cq_read", (ret<0), ret);
		completed += ret;
	}
}

static void send_one(int size)
{	
	int err;
	auto s = std::chrono::high_resolution_clock::now();
	// CHK_ERR("send_one", (peer_addr==0ULL), -EDESTADDRREQ);
	std::cout << "Send to peer " << peer_addr << "\n";
	size_t len = 64;

	// err = fi_send(ep, sbuf, size, NULL, peer_addr, &sctxt);
	// err = fi_tsend(ep, sbuf, size, NULL, peer_addr, 123, NULL);
	// err = fi_tsend(ep, largebuff, largeSize, NULL, peer_addr, 123, NULL);
	err = fi_send(ep, largebuff, largeSize, NULL, peer_addr, &sctxt);
	CHK_ERR("fi_send", (err<0), err);

	wait_cq();
	auto e = std::chrono::high_resolution_clock::now();
	std::chrono::duration<double, std::milli> cost_t = e - s;
	float dur = cost_t.count();
	std::cout << "Send message cost :" << dur << " ms\n";
	std::cout << "send bw " << ((largeSize * 8) / (dur / 1000.0)) / 1e9 << " Gbps\n";
}

static void recv_one(int size)
{
	int err;
	auto s = std::chrono::high_resolution_clock::now();
	// err = fi_recv(ep, rbuf, size, NULL, FI_ADDR_UNSPEC, &rctxt);
	// err = fi_trecv(ep, rbuf, size, NULL, FI_ADDR_UNSPEC, 123, 0, NULL);
	// err = fi_trecv(ep, largebuff, largeSize, NULL, FI_ADDR_UNSPEC, 123, 0, NULL);
	err = fi_recv(ep, largebuff, largeSize, NULL, FI_ADDR_UNSPEC, &rctxt);
	CHK_ERR("fi_recv", (err<0), err);

	wait_cq();
	auto e = std::chrono::high_resolution_clock::now();
	std::chrono::duration<double, std::milli> cost_t = e - s;
	float dur = cost_t.count();
	std::cout << "Recv message cost :" << dur << " ms\n";
	std::cout << "recv bw " << ((largeSize * 8) / (dur / 1000.0)) / 1e9 << " Gbps\n";

}



int main(int argc, char *argv[])
{
	/* you need to modify the addr_dict for your own case*/
	char addr_dict[6][17]  = {
		{-2, -128, 0, 0, 0, 0, 0, 0, 0, -7, 127, -1, -2, -89, -77, -119, 0},
		{-2, -128, 0, 0, 0, 0, 0, 0, 0, -7, 127, -1, -2, -89, -77, -119, 1},
		{-2, -128, 0, 0, 0, 0, 0, 0, 0, -7, 127, -1, -2, -89, -77, -119, 2},
		{-2, -128, 0, 0, 0, 0, 0, 0, 0, 80, 48, -1, -2, 118, -13, -9, 0},
		{-2, -128, 0, 0, 0, 0, 0, 0, 0, 80, 48, -1, -2, 118, -13, -9, 1},
		{-2, -128, 0, 0, 0, 0, 0, 0, 0, 80, 48, -1, -2, 118, -13, -9, 2}
	};

	largebuff = new char[largeSize];
	ft_fill_buf(largebuff, largeSize);

	int size = 64;

	if (argc < 2) {
		std::cout << "Usage, require endpoint nick name, e.g. ./fi_hello ep0 \n";
		return 1;
	}
	nickname = std::string(argv[1]);
	std::string msg = "Hi server, this is "+nickname;
	strcpy(sbuf, msg.c_str());

	std::cout << "Input running mode: 0 stand for server, 1 for client\n";
	std::cin >> is_client;
	if (is_client != 0 && is_client != 1) {std::cerr << "must be 0/1"; return 1;}

	init_fabric();

	std::cout << "Endpoint initialized, input idx for peer, avialble index: 0-5\n" ;
	int peer_idx = 0;
	std::cin >> peer_idx;

	char dst_addr[size];
	memset(dst_addr, 0, size);
	// copy address
	for (int i = 0; i < sizeof(addr_dict[peer_idx]); i ++) {
		dst_addr[i] = addr_dict[peer_idx][i];
	}
	// insert dst addr
	int numSuccess = 0;
	numSuccess = fi_av_insert(av, dst_addr, 1, &peer_addr, 0, NULL);
	CHK_ERR("fi_av_insert", (numSuccess!=1), numSuccess);

	// ------ sentinel check inserted address
	char newbuf[64], newAddr[64]; size_t len = 64; int err = 0;
	memset(newbuf, 0, 64);
	memset(newAddr, 0, 64);
	err = fi_av_lookup(av, peer_addr, newAddr, &len);
	printf("look up error code %d\n retrieved addr buffer: \n", err);
	for (int i = 0; i < 64; i ++){
		printf("%d, ", newAddr[i]);
	}
	printf("\n");
	// 
	// get the readable address for debugging;
	fi_av_straddr(av, newAddr, newbuf, &len);
	printf("Send to peer address %s\n", newbuf);
	// ------ address check done
	
	int repeat = 20;
	if (is_client) {
		printf("Sending '%s' to server\n", sbuf);
		for (int i = 0; i < repeat; i++) {
			send_one(size);
			recv_one(size);
			std::this_thread::sleep_for(std::chrono::seconds(5));
		}
		printf("Received '%s' from server\n", rbuf);

	} else {
		printf("Waiting for client\n");
		while (1) {
			recv_one(size);
			// printf("Received '%s' from client\n", rbuf);
			// printf("Sending '%s' to client\n", sbuf);
			send_one(size);
			// printf("Done, press RETURN to continue, 'q' to exit\n");
			// if (getchar() == 'q')
			// 	break;
		}
	}

	finalize_fabric();
	delete[] largebuff;
	return 0;
}
