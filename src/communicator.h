#ifndef COMMUNICATOR_H
#define COMMUNICATOR_H

#include <string>

#include <rdma/fabric.h>
#include <rdma/fi_domain.h>

namespace trans {

class Communicator {
    struct fi_info *infos, *hints;
    struct fid_fabric *fabric;
    struct fid_domain *domain;

    struct fid_cq *txcq, *rxcq;
    struct fid_cntr *txcntr, *rxcntr;
    struct fi_context tx_ctx, rx_ctx;

    uint64_t tx_seq, rx_seq, tx_cq_cntr, rx_cq_cntr;
    size_t buf_size, tx_size, rx_size, tx_mr_size, rx_mr_size;

    struct fi_av_attr av_attr;
    struct fi_eq_attr eq_attr;
    struct fi_cq_attr cq_attr;
    struct fi_cntr_attr cntr_attr;

    int _parseinfo();
    int _init_cq_data();
    int _init_buf();
    int _init_fabric();
    int _open_fabric_res();


public:
    Communicator(std::string src, std::string dst);

    void send(void* buf, uint64_t size);
    void recv(void* buf, uint64_t size);
};


class EFAComm : public Communicator {

public:
    EFAComm(std::string srcGID, std::string dstGID);
};

};


#endif /* COMMUNICATOR_H */
