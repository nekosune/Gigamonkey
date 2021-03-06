// Copyright (c) 2021 Daniel Krawisz
// Distributed under the Open BSV software license, see the accompanying file LICENSE.

#ifndef GIGAMONKEY_LEDGER
#define GIGAMONKEY_LEDGER

#include "spv.hpp"
#include <gigamonkey/script/script.hpp>

namespace Gigamonkey::Bitcoin {
    
    struct ledger {
        using block_header = Bitcoin::headers::header;
        
        virtual list<block_header> headers(uint64 since_height) = 0;
        
        struct double_entry : ptr<bytes> {
            Merkle::proof Proof;
            header Header;
            
            bool valid() const {
                return *this != nullptr;
            }
            
            bool confirmed() const {
                return valid() && Header.valid() && id() == Proof.Branch.Leaf.Digest && Proof.valid() && Proof.Root == Header.MerkleRoot;
            }
            
            double_entry() : ptr<bytes>{nullptr}, Proof{}, Header{} {}
            
            // for a confirmed transaction. 
            double_entry(ptr<bytes> t, Merkle::proof p, const header& h) : ptr<bytes>{t}, Proof{p}, Header{h} {}
            
            // for an unconfirmed transaction. 
            double_entry(ptr<bytes> t) : ptr<bytes>{t}, Proof{}, Header{} {}
            
            bool operator==(const double_entry &t) const;
            bool operator!=(const double_entry &t) const;
            bool operator<=(const double_entry& t) const;
            bool operator>=(const double_entry& t) const;
            bool operator<(const double_entry& t) const;
            bool operator>(const double_entry& t) const;
            
            Bitcoin::output output(uint32) const;
            Bitcoin::input input(uint32) const;
            
            list<Bitcoin::output> outputs() const {
                return operator Bitcoin::transaction().Outputs;
            }
            
            list<Bitcoin::input> inputs() const {
                return operator Bitcoin::transaction().Inputs;
            }
            
            txid id() const {
                return Bitcoin::transaction::id(this->operator*());
            }
            
            satoshi sent() const;
            
            timestamp time() const {
                return Header.Timestamp;
            }
            
            explicit operator Bitcoin::transaction() const {
                return Bitcoin::transaction::read(this->operator*());
            }
            
        };
        
        virtual data::entry<txid, double_entry> transaction(const txid&) const = 0;
        
        // get header by header hash and merkle root.
        virtual block_header header(const digest256&) const = 0; 
        
        // get block by header hash and merkle root. 
        virtual bytes block(const digest256&) const = 0; 
        
        struct vertex;
        
        struct prevout {
            data::entry<txid, double_entry> Previous;
            
            uint32_little Index;
            input Input;
            
            explicit operator output() const {
                return Previous.Value.output(Input.Outpoint.Index);
            }
            
            satoshi spent() const {
                return output(*this).Value;
            }
            
            bool valid() const {
                return Previous.Value.valid() && 
                    Input.Outpoint.Reference == Previous.Key/* && 
                    evaluate_script(output(*this).Script, Input.Script).valid()*/;
            }
            
        private:
            prevout() : Previous{txid{}}, Index{}, Input{} {}
            prevout(data::entry<txid, double_entry> p, uint32_little i, input in) : Previous{p}, Index{i}, Input{in} {}
            
            friend struct vertex;
        };
        
        struct vertex : public double_entry {
            data::map<txid, double_entry> Previous;
            
            satoshi spent() const {
                return data::fold([](satoshi x, const prevout& p) -> satoshi {
                    return p.spent();
                }, satoshi{0}, prevouts());
            }
            
            satoshi fee() const {
                return spent() - sent();
            }
            
            bool valid() const;
            
            uint32 sigops() const;
            
            list<prevout> prevouts() const {
                list<prevout> p;
                list<Bitcoin::input> inputs = double_entry::inputs();
                index i = 0;
                for (const Bitcoin::input& in : inputs) p = p << 
                    prevout{data::entry<txid, double_entry>{in.Outpoint.Reference, Previous[in.Outpoint.Reference]}, i++, in};
                return p;
            }
            
            vertex(const double_entry& d, data::map<txid, double_entry> p) : double_entry{d}, Previous{p} {}
            vertex() : double_entry{}, Previous{} {}
            
            prevout operator[](index i) {
                struct input in = double_entry::input(i);
                
                return {data::entry<txid, double_entry>{in.Outpoint.Reference, Previous[in.Outpoint.Reference]}, i, in};
            }
        };
        
        vertex make_vertex(const double_entry& d) {
            list<input> in = d.inputs();
            data::map<txid, double_entry> p;
            for (const input& i : in) p = p.insert(transaction(i.Outpoint.Reference));
            return {d, p};
        }
    
        struct edge {
            input Input;
            output Output;
        
            bool valid() const {
                return Output.valid() && Input.valid();
            } 
        };
        
    };
    
    struct timechain : ledger {
        
        virtual bool broadcast(const bytes_view&) = 0;
        
    };
    
    bool inline ledger::double_entry::operator==(const double_entry &t) const {
        // if the types are valid then checking this proves that they are equal. 
        return Header == t.Header && Proof.index() == t.Proof.index();
    }
    
    bool inline ledger::double_entry::operator!=(const double_entry &t) const {
        return !(*this == t);
    }
    
    bool inline ledger::double_entry::operator<=(const double_entry& t) const {
        if (Header == t.Header) return Proof.index() <= t.Proof.index();
        return Header <= t.Header;
    }
    
    bool inline ledger::double_entry::operator>=(const double_entry& t) const {
        if (Header == t.Header) return Proof.index() >= t.Proof.index();
        return Header >= t.Header;
    }
    
    bool inline ledger::double_entry::operator<(const double_entry& t) const {
        if (Header == t.Header) return Proof.index() < t.Proof.index();
        return Header < t.Header;
    }
    
    bool inline ledger::double_entry::operator>(const double_entry& t) const {
        if (Header == t.Header) return Proof.index() > t.Proof.index();
        return Header > t.Header;
    }
    
    Bitcoin::output inline ledger::double_entry::output(uint32 i) const {
        auto t = Bitcoin::transaction::read(ptr<bytes>::operator*());
        if (!t.valid() || t.Outputs.size() <= i) return {};
        return t.Outputs[i];
    }
    
    Bitcoin::input inline ledger::double_entry::input(uint32 i) const {
        auto t = Bitcoin::transaction::read(ptr<bytes>::operator*());
        if (!t.valid() || t.Inputs.size() <= i) return {};
        return t.Inputs[i];
    }
    
}

#endif
