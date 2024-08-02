#ifndef __M_MATHCHER_H__
#define __M_MATHCHER_H__


#include"util.hpp"
#include"db.hpp"
#include"room.hpp"
#include"online.hpp"
#include<condition_variable>
#include<list>
#include<mutex>
#include"logger.hpp"


template<class T>
class match_queue{
    private:
        std::list<T> _list;
        std::mutex _mutex;
        std::condition_variable _cond;
    public:
        match_queue(){}
        ~match_queue(){}
        int size(){
            std::unique_lock<std::mutex> lock(_mutex);
            return _list.size();
        }
        bool empty(){
            std::unique_lock<std::mutex> lock(_mutex);
            return _list.empty();
        }
        void wait(){
            std::unique_lock<std::mutex> lock(_mutex);
            _cond.wait(lock);
        }
        void push(const T&data){
            std::unique_lock<std::mutex> lock(_mutex);
            _list.push_back(data);
            _cond.notify_all();
        }
        bool pop(T& data){
            std::unique_lock<std::mutex> lock(_mutex);
            if(_list.empty() == true){
                return false;
            }
            data = _list.front();
            _list.pop_front();
            return true;
        }
        
        void remove(T& data){
            std::unique_lock<std::mutex> lock(_mutex);
            _list.remove(data);
        }







};



class matcher{
    private:
        match_queue<uint64_t> _q_normal;
        match_queue<uint64_t> _q_high;  
        match_queue<uint64_t> _q_super;
        std::thread _th_normal;
        std::thread _th_high;
        std::thread _th_super;
        room_manager* _rm;
        user_table* _ut;
        online_manager* _om;
        private:
            void handle_match(match_queue<uint64_t> &mq){
                while(1){
                //1. 判断队列人数是否大于2，<2则阻塞等待
                    while(mq.size() <2 ){
                        mq.wait();
                    }
                //2. 走下来代表人数够了，出队两个玩家
                    uint64_t uid1,uid2;
                    bool ret = mq.pop(uid1);
                    if(ret == false){continue;}
                    ret = mq.pop(uid2);
                    if(ret == false){this->add(uid1);continue;}
                //3. 校验两个玩家是否在线，如果有人掉线，则把另一个人重新加入队列
                    server_t::connection_ptr conn1 = _om->get_conn_from_hall(uid1);
                    if(conn1.get() == nullptr ){
                        this->add(uid2);
                        continue;
                    }
                    server_t::connection_ptr conn2 = _om->get_conn_from_hall(uid2);
                    if(conn2.get() == nullptr ){
                        this->add(uid1);
                        continue;
                    }

                //4. 为两个玩家创建房间，并将玩家加入房间中
                    room_ptr rp = _rm->create_room(uid1,uid2);
                    if(rp.get() == nullptr){
                        this->add(uid1);
                        this->add(uid2);
                        continue;
                    }
                // 5. 对两个玩家进行响应
                    Json::Value json_resp;
                    json_resp["optype"] = "match_success";
                    json_resp["result"] = true;
                    std::string body;
                    json_util::serialize(json_resp,body);
                    conn1->send(body);
                    conn2->send(body);
                }

            }
            void th_normal_entry(){return handle_match(_q_normal);}
            void th_high_entry(){return handle_match(_q_high);}
            void th_super_entry(){return handle_match(_q_super);}



        public:
            matcher(room_manager* rm,user_table* ut,online_manager* om):_rm(rm),_ut(ut),_om(om),
            _th_normal(std::thread(&matcher::th_normal_entry,this)),
            _th_high(std::thread(&matcher::th_high_entry,this)),
            _th_super(std::thread(&matcher::th_super_entry,this)){
                DLOG("游戏匹配模块初始化成功!");
                }
            bool add(uint64_t uid){
                Json::Value user;
                bool ret= _ut->select_by_id(uid,user);
                if(ret == false){
                    DLOG("获取玩家: %ld 信息失败", uid);
                    return false;
                }
                int score = user["score"].asInt();
                if(score <2000 )
                {
                    _q_normal.push(uid);
                }else if(score >= 2000  && score<3000){
                    _q_high.push(uid);

                }else if(score >2000){
                    _q_super.push(uid);

                }
                return true;
            }

            bool del(uint64_t uid){
                Json::Value user;
                bool ret= _ut->select_by_id(uid,user);
                if(ret == false){
                    DLOG("获取玩家: %ld 信息失败", uid);
                    return false;
                }
                int score = user["score"].asInt();
                if(score <2000 )
                {
                    _q_normal.remove(uid);
                }else if(score >= 2000  && score<3000){
                    _q_high.remove(uid);

                }else if(score >2000){
                    _q_super.remove(uid);
                }
                return true;

            }




};
#endif