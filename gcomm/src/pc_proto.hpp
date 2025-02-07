/*
 * Copyright (C) 2009 Codership Oy <info@codership.com>
 */

#ifndef GCOMM_PC_PROTO_HPP
#define GCOMM_PC_PROTO_HPP

#include <list>
#include <ostream>

#include "gcomm/uuid.hpp"
#include "gcomm/protolay.hpp"
#include "gcomm/conf.hpp"
#include "pc_message.hpp"
#include "defaults.hpp"

#include "gu_uri.hpp"
#include "gu_mutex.hpp"
#include "gu_cond.hpp"


namespace gcomm
{
    namespace pc
    {
        class Proto;
        class ProtoBuilder;
        std::ostream& operator<<(std::ostream& os, const Proto& p);
    }
}


class gcomm::pc::Proto : public Protolay
{
public:

    enum State
    {
        S_CLOSED,
        S_STATES_EXCH,
        S_INSTALL,
        S_PRIM,
        S_TRANS,
        S_NON_PRIM,
        S_MAX
    };

    static std::string to_string(const State s)
    {
        switch (s)
        {
        case S_CLOSED:      return "CLOSED";
        case S_STATES_EXCH: return "STATES_EXCH";
        case S_INSTALL:     return "INSTALL";
        case S_TRANS:       return "TRANS";
        case S_PRIM:        return "PRIM";
        case S_NON_PRIM:    return "NON_PRIM";
        default:
            gu_throw_fatal << "Invalid state";
        }
    }


    Proto(gu::Config&    conf,
          const UUID&    uuid,
          SegmentId      segment,
          const gu::URI& uri = gu::URI("pc://"),
          View*          rst_view = NULL)
        :
        Protolay(conf),
        my_uuid_          (uuid),
        start_prim_       (),
        npvo_             (param<bool>(conf, uri, Conf::PcNpvo, Defaults::PcNpvo)),
        ignore_quorum_    (param<bool>(conf, uri, Conf::PcIgnoreQuorum,
                                       Defaults::PcIgnoreQuorum)),
        ignore_sb_        (param<bool>(conf, uri, Conf::PcIgnoreSb,
                                       gu::to_string(ignore_quorum_))),
        closing_          (false),
        state_            (S_CLOSED),
        last_sent_seq_    (0),
        checksum_         (param<bool>(conf, uri, Conf::PcChecksum,
                                       Defaults::PcChecksum)),
        instances_        (),
        self_i_           (instances_.insert_unique(std::make_pair(uuid, Node()))),
        state_msgs_       (),
        current_view_     (0, V_NONE),
        pc_view_          (0, V_NON_PRIM),
        views_            (),
        mtu_              (std::numeric_limits<int32_t>::max()),
        weight_           (check_range(Conf::PcWeight,
                                      param<int>(conf, uri, Conf::PcWeight,
                                                 Defaults::PcWeight),
                                      0, 0xff)),
        rst_view_         (),
        sync_param_mutex_ (0),
        sync_param_cond_  (0),
        param_sync_set_   (0)

    {
        set_weight(weight_);
        NodeMap::value(self_i_).set_segment(segment);
        if (rst_view) {
            set_restored_view(rst_view);
        }

        conf.set(Conf::PcNpvo,         gu::to_string(npvo_));
        conf.set(Conf::PcIgnoreQuorum, gu::to_string(ignore_quorum_));
        conf.set(Conf::PcIgnoreSb,     gu::to_string(ignore_sb_));
        conf.set(Conf::PcChecksum,     gu::to_string(checksum_));
        conf.set(Conf::PcWeight,       gu::to_string(weight_));
    }

    ~Proto() { }

    const UUID& uuid() const { return my_uuid_; }

    bool prim() const { return NodeMap::value(self_i_).prim(); }

    void set_prim(const bool val) { NodeMap::value(self_i_).set_prim(val); }

    void mark_non_prim();


    const ViewId& last_prim() const
    { return NodeMap::value(self_i_).last_prim(); }

    void set_last_prim(const ViewId& vid)
    {
        gcomm_assert(vid.type() == V_PRIM);
        NodeMap::value(self_i_).set_last_prim(vid);
    }

    uint32_t last_seq() const
    { return NodeMap::value(self_i_).last_seq(); }

    void set_last_seq(const uint32_t seq)
    { NodeMap::value(self_i_).set_last_seq(seq); }

    int64_t to_seq() const
    { return NodeMap::value(self_i_).to_seq(); }

    void set_to_seq(const int64_t seq)
    { NodeMap::value(self_i_).set_to_seq(seq); }

    void set_weight(int weight)
    { NodeMap::value(self_i_).set_weight(weight); }


    class SMMap : public Map<UUID, Message> { };

    const View& current_view() const { return current_view_; }

    const UUID& self_id() const { return my_uuid_; }

    State       state()   const { return state_; }

    void shift_to    (State);
    void send_state  ();
    int  send_install(bool bootstrap, int weight = -1);

    void handle_first_trans (const View&);
    void handle_trans       (const View&);
    void handle_reg         (const View&);

    void handle_msg  (const Message&, const Datagram&,
                      const ProtoUpMeta&);
    void handle_up   (const void*, const Datagram&,
                      const ProtoUpMeta&);
    int  handle_down (Datagram&, const ProtoDownMeta&);

    void connect(bool first)
    {
        log_debug << self_id() << " start_prim " << first;
        start_prim_ = first;
        closing_    = false;
        shift_to(S_NON_PRIM);
    }

    void close(bool force = false) { closing_ = true; }

    void handle_view (const View&);

    bool set_param(const std::string& key, const std::string& val, 
                   Protolay::sync_param_cb_t& sync_param_cb);
    
    void sync_param();

    void set_mtu(size_t mtu) { mtu_ = mtu; }
    size_t mtu() const { return mtu_; }
    void set_restored_view(View* rst_view) {
        gcomm_assert(state_ == S_CLOSED);
        rst_view_ = rst_view;
        NodeMap::value(self_i_).set_last_prim(
            // set last prim just for exchanging uuid and seq.
            // but actually restored view is not actual prim view.
            ViewId(V_NON_PRIM,
                   rst_view -> id().uuid(),
                   rst_view -> id().seq()));
    }
    const View* restored_view() const { return rst_view_; }
    int cluster_weight() const;
private:
    friend std::ostream& operator<<(std::ostream& os, const Proto& p);
    // Helper class to construct Proto states for unit tests
    friend class ProtoBuilder;
    Proto(gu::Config& conf, const UUID& uuid)
        :
        Protolay(conf),
        my_uuid_          (uuid),
        start_prim_       (),
        npvo_             (),
        ignore_quorum_    (),
        ignore_sb_        (),
        closing_          (),
        state_            (),
        last_sent_seq_    (),
        checksum_         (),
        instances_        (),
        self_i_           (),
        state_msgs_       (),
        current_view_     (0, V_NONE),
        pc_view_          (0, V_NON_PRIM),
        views_            (),
        mtu_              (std::numeric_limits<int32_t>::max()),
        weight_           (),
        rst_view_         (),
        sync_param_mutex_ (0),
        sync_param_cond_  (0),
        param_sync_set_   (0)
    { }

    Proto (const Proto&);
    Proto& operator=(const Proto&);

    bool requires_rtr() const;
    bool is_prim() const;
    bool have_quorum(const View&, const View&) const;
    bool have_split_brain(const View&) const;
    void validate_state_msgs() const;
    void cleanup_instances();
    void handle_state(const Message&, const UUID&);
    void handle_install(const Message&, const UUID&);
    void handle_trans_install(const Message&, const UUID&);
    void handle_user(const Message&, const Datagram&,
                     const ProtoUpMeta&);
    void deliver_view(bool bootstrap = false);

    UUID   const      my_uuid_;       // Node uuid
    bool              start_prim_;    // Is allowed to start in prim comp
    bool              npvo_;          // Newer prim view overrides
    bool              ignore_quorum_; // Ignore lack of quorum
    bool              ignore_sb_;     // Ignore split-brain condition
    bool              closing_;       // Protocol is in closing stage
    State             state_;         // State
    uint32_t          last_sent_seq_; // Msg seqno of last sent message
    bool              checksum_;      // Enable message checksumming
    NodeMap           instances_;     // Map of known node instances
    NodeMap::iterator self_i_;        // Iterator pointing to self node instance

    SMMap             state_msgs_;    // Map of received state messages
    View              current_view_;  // EVS view
    View              pc_view_;       // PC view
    std::list<View>   views_;         // List of seen views
    size_t            mtu_;           // Maximum transmission unit
    int               weight_;        // Node weight in voting
    View*             rst_view_;      // restored PC view

    gu::Mutex         sync_param_mutex_;
    gu::Cond          sync_param_cond_;
    bool              param_sync_set_;
};

class gcomm::pc::ProtoBuilder
{
public:
    ProtoBuilder()
        : conf_()
        , uuid_()
        , state_msgs_()
        , current_view_()
        , pc_view_()
        , instances_()
        , state_(Proto::S_CLOSED)
    { }
    Proto* make_proto()
    {
        gcomm_assert(uuid_ != UUID::nil());
        Proto* ret(new Proto(conf_, uuid_));
        ret->state_msgs_ = state_msgs_;
        ret->current_view_ = current_view_;
        ret->pc_view_ = pc_view_;
        ret->instances_ = instances_;
        ret->self_i_ = ret->instances_.find_checked(uuid_);
        ret->state_ = state_;

        return ret;
    }
    ProtoBuilder& conf(const gu::Config& conf)
    { conf_ = conf; return *this; }
    ProtoBuilder& uuid(const gcomm::UUID& uuid)
    { uuid_ = uuid; return *this; }
    ProtoBuilder& state_msgs(const Proto::SMMap& state_msgs)
    { state_msgs_ = state_msgs; return *this; }
    ProtoBuilder& current_view(const gcomm::View& current_view)
    { current_view_ = current_view; return *this; }
    ProtoBuilder& pc_view(const gcomm::View& pc_view)
    { pc_view_ = pc_view; return *this; }
    ProtoBuilder& instances(const NodeMap& instances)
    { instances_ = instances; return *this; }
    ProtoBuilder& state(enum Proto::State state)
    { state_ = state; return *this; }
private:
    gu::Config conf_;
    gcomm::UUID uuid_;
    Proto::SMMap state_msgs_;
    gcomm::View current_view_;
    gcomm::View pc_view_;
    NodeMap instances_;
    enum Proto::State state_;
};

#endif // PC_PROTO_HPP
