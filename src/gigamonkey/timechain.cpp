// Copyright (c) 2019 Daniel Krawisz
// Distributed under the Open BSV software license, see the accompanying file LICENSE.

#include <gigamonkey/timechain.hpp>

namespace gigamonkey {
    bool header_valid_work(slice<80> h) {
        return work::candidate::valid(h);
    }
    
    bool header_valid(const bitcoin::header& h) {
        return h.Version >= 1 && h.MerkleRoot.valid() && h.Timestamp != timestamp{};
    }
}

namespace gigamonkey::header {
    
    bool valid(slice<80> h) {
        return header_valid(bitcoin::header::read(h)) && header_valid_work(h);
    }
    
}

namespace gigamonkey::transaction {
    bool valid(bytes_view) {
        throw data::method::unimplemented{"transaction::valid"};
    }
    
    // Whether this is a coinbase transaction. 
    bool coinbase(bytes_view) {
        throw data::method::unimplemented{"transaction::coinbase"};
    }
}

namespace gigamonkey::block {
        
    bool valid(bytes_view b) {
        slice<80> h = header(b);
        if (!header::valid(h)) return false;
        vector<bytes_view> txs = transactions(b);
        if (txs.size() == 0 || !transaction::coinbase(txs[0])) return false;
        for (int i = 1; i < txs.size(); i++) if (!transaction::valid(txs[i])) return false;
        return digest<32>{header::merkle_root(h)} == merkle_root(txs);
    }
    
    vector<bytes_view> transactions(bytes_view) {
        throw data::method::unimplemented{"block::transactions"};
    }
    
    slice<80> header(bytes_view) {
        throw data::method::unimplemented{"block::header"};
    }
    
}

namespace gigamonkey::bitcoin {
        
    bool header::valid() const {
        return header_valid_work(write()) && header_valid(*this);
    }
    
}

