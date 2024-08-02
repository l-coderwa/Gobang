#ifndef __M_SS_H__
#define __M_SS_H__

#include"util.hpp"
#include<unordered_map>
#include<memory>


typedef websocketpp::server<websocketpp::config::asio> server_t;

typedef enum {UNLOGIN , LOGIN} ss_statu;

class session{
    private:
        uint64_t _ssid;
        uint64_t _uid;
        ss_statu _statu;
        server_t::timer_ptr _tp;
    public:
        session(uint64_t ssid): _ssid(ssid) {DLOG("SESSION %p 被创建", this); }
        ~session(){DLOG("SESSION %p 被释放", this);}
        void set_user(uint64_t uid){ _uid=uid; }
        bool set_statu(ss_statu statu){return (_statu = statu);}
        uint64_t get_user(){return _uid;}
        bool is_login(){return (_statu == LOGIN);}
        void set_timer(const server_t::timer_ptr &tp){_tp=tp;}
        server_t::timer_ptr & get_timer(){ return _tp;}
        uint64_t ssid(){return _ssid;}



};


#define SESSION_TIMEOUT 3000
#define SESSION_FOREVER -1
using session_ptr = std::shared_ptr<session>; 
class session_manager{
    private:
        uint64_t _next_ssid;
        std::mutex _mutex;
        std::unordered_map<uint64_t,session_ptr> _session;
        server_t* _server;
    public:
    session_manager(server_t* server):_next_ssid(1),_server(server)
    {
        DLOG("session 管理器初始化完毕！");
    }
    ~session_manager(){DLOG("session管理器即将销毁！");}

    
    session_ptr create_session(uint64_t uid,ss_statu statu){
        std::unique_lock<std::mutex> lock(_mutex);
        session_ptr ssp (new session(_next_ssid));
        
        ssp->set_statu(statu);
        ssp->set_user(uid);
        _session.insert(std::make_pair(_next_ssid, ssp));
        _next_ssid++;
        return ssp;
    }

    void append_session(const session_ptr &ssp){
        std::unique_lock<std::mutex> lock(_mutex);
        _session.insert(std::make_pair(ssp->ssid(), ssp));

    }
    session_ptr get_session_by_ssid(uint64_t ssid){
        std::unique_lock<std::mutex> lock(_mutex);
        auto it = _session.find(ssid);
        if(it == _session.end()){
            return session_ptr();
        }
        return it->second;
        
    }
    void remove_session(uint64_t ssid){
        std::unique_lock<std::mutex> lock(_mutex);
        _session.erase(ssid);

    }

    void set_session_expire_time(uint64_t ssid,int ms){
        session_ptr ssp = get_session_by_ssid(ssid);
        if(ssp.get() == nullptr){
            return;
        }
        server_t::timer_ptr tp = ssp->get_timer();
        if(tp.get() == nullptr && ms == SESSION_FOREVER)
        //1. 在 session 永久存在的情况下，设置永久存在
        {
            return;
        }
        else if(tp.get() == nullptr&& ms != SESSION_FOREVER)
        // 2. 在session 永久存在的情况下，设置指定时间之后被删除的任务
        {
            //添加定时任务
            server_t::timer_ptr tmp_tp = _server->set_timer(ms,
             std::bind(&session_manager::remove_session,this , ssid));
            ssp->set_timer(tmp_tp);
        }   
        else if(tp.get() != nullptr && ms == SESSION_FOREVER)
        //3. 在session设置了定时删除的情况下，将session设置为永久存在
        {
            // 删除定时任务
            tp->cancel();
            ssp->set_timer(server_t::timer_ptr());
            _server->set_timer(0,std::bind
            (&session_manager::append_session,this,ssp));
        }
 
        // 4. 在session 设置了定时删除的情况下，将session重置删除时间
        else if(tp.get() != nullptr && ms != SESSION_FOREVER){
             tp->cancel();

            ssp->set_timer(server_t::timer_ptr());
             _server->set_timer(0, std::bind
             (&session_manager::append_session,this , ssp));

            server_t::timer_ptr tmp_tp = _server->set_timer(ms,
             std::bind(&session_manager::remove_session,this , ssp->ssid()));
            ssp->set_timer(tmp_tp);
        }
    }
};

#endif




