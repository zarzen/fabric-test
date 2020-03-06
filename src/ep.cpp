#include <iostream>


#include <rdma/fabric.h>
#include <rdma/fi_domain.h>
#include <rdma/fi_endpoint.h>
#include <rdma/fi_cm.h>

class EP {
    std::string nickname;
    bool isServer = true;
    struct fi_info		*fi;
    struct fid_fabric	*fabric;
    struct fid_domain	*domain;
    struct fid_av		*av;
    struct fid_ep		*ep;
    struct fid_cq		*cq;
    fi_addr_t		peer_addr;

    char			sbuf[64];
    char			rbuf[64];
    size_t max_size = 64;

public:
    EP(std::string nickname) {
        this->nickname = nickname;
    };

    int init_res(){
        struct fi_info		*hints;
        struct fi_cq_attr	cq_attr;
        struct fi_av_attr	av_attr;
        int err;

        hints = fi_allocinfo();
        if (!hints) std::cerr << "fi_allocinfo err " << -ENOMEM << "\n";

        // clear all buffers
        memset(&cq_attr, 0, sizeof(cq_attr));
        memset(&av_attr, 0, sizeof(av_attr));

        // get provider
        hints->ep_attr->type = FI_EP_RDM;
        hints->fabric_attr->prov_name = strdup("efa");
        err = fi_getinfo(FI_VERSION(1, 9), NULL, NULL, 0, hints, &fi);
        if (err < 0) std::cerr << "fi_getinfo err " << err << "\n";

        fi_freeinfo(hints);
        printf("Using OFI device: %s\n", fi->fabric_attr->name);

        // init fabric, domain, address-vector, 
        err = fi_fabric(fi->fabric_attr, &fabric, NULL);
        if (err < 0) std::cerr << "fi_fabric err " << err << "\n";
        err = fi_domain(fabric, fi, &domain, NULL);
        if (err < 0) std::cerr << "fi_domain err " << err << "\n";

        av_attr.type = fi->domain_attr->av_type;
        av_attr.count = 3;
        err = fi_av_open(domain, &av_attr, &av, NULL);
        if (err < 0) std::cerr << "fi_av_open err " << err << "\n";

        cq_attr.format = FI_CQ_FORMAT_TAGGED;
        cq_attr.size = 100;
        err = fi_cq_open(domain, &cq_attr, &cq, NULL);
        if (err < 0) std::cerr << "fi_cq_open err " << err << "\n";

        err = fi_endpoint(domain, fi, &ep, NULL);
        if (err < 0) std::cerr << "fi_endpoint err " << err << "\n";

        err = fi_ep_bind(ep, (fid_t)cq, FI_SEND|FI_RECV);
        if (err < 0) std::cerr << "fi_ep_bind cq err " << err << "\n";

        err = fi_ep_bind(ep, (fid_t)av, 0);
        if (err < 0) std::cerr << "fi_ep_bind av err " << err << "\n";

        err = fi_enable(ep);
    	if (err < 0) std::cerr << "fi_enable err " << err << "\n";
    };

    void write_out_self_addr() {
        int err = 0;
        unsigned char name[max_size]; 
        char buf[max_size];
        // get name/self-address
        err = fi_getname((fid_t)ep, name, &max_size);
        if (err < 0) std::cerr << "fi_getname err " << err << "\n";
        // print out the the name buff
        std::cout << "Name buffer: ";
        for (int i = 0; i < max_size; i++) {
            std::cout << unsigned(name[i]) << ", ";
        }
        std::cout << "\n";
        // print translate the addr in reable format
        fi_av_straddr(av, name, buf, &max_size);
        std::cout << "Readable self address: " << buf << "\n";
        
    };

    void insert_peer_address(std::string addr_file) {

    }

    void client_mode() {
        isServer = false;
    }

    void run() {
        if (isServer)
            _as_server();
        else
            _as_client();
    }

    void recv_one() {

    }

    void send_one(std::string msg) {

    }

    void _as_server(){
        while (1) {
			std::cout << "Waiting for client\n";
			recv_one();
			printf("Received '%s' from client\n", rbuf);
			printf("Sending '%s' to client\n", sbuf);
			send_one("hello from server, " + nickname);

			printf("Done, press RETURN to continue, 'q' to exit\n");
			if (getchar() == 'q')
				break;
		}
    }

    void _as_client() {
        while (1) {
            std::cout << "input message to send to peer\n";
            std::string msg;
            std::getline(std::cin, msg);
            send_one(msg);
            recv_one();

            printf("Done, press RETURN to continue, 'q' to exit\n");
			if (getchar() == 'q')
				break;
        }
    }

    ~EP() {
        fi_close((fid_t)ep);
        fi_close((fid_t)cq);
        fi_close((fid_t)av);
        fi_close((fid_t)domain);
        fi_close((fid_t)fabric);
        fi_freeinfo(fi);
    }
};

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cout << "specify nickname of your endpoint \n";
        return 1;
    }

    EP ep = EP(std::string(argv[1]));
    ep.init_res();
    ep.write_out_self_addr();

    std::cout << "Input filepath of peer addrs to contine\n";
    std::string peer_addr_file;
    std::getline(std::cin, peer_addr_file);
    ep.insert_peer_address(peer_addr_file);

    std::cout << "Input endpoint mode for usage: 0 for server; 1 for client\n";
    std::string mode;
    std::getline(std::cin, mode);
    if (mode == "1") {
        ep.client_mode();
    } else if (mode != "0") {
        std::cout << "wrong input format, endpoint runs in server mode";
    }

    std::cout << "input any key to start working\n";
    system("pause");
    ep.run();

    return 0;
}