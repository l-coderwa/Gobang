#ifndef __M_ROOM_H__
#define __M_ROOM_H__
#include"util.hpp"
#include"logger.hpp"
#include"online.hpp"
#include"db.hpp"


typedef enum {GAME_START, GAME_OVER} room_statu;
#define BOARD_ROW 15
#define BOARD_COL 15
#define WHITE_CHESS 1
#define BLACK_CHESS 2

class room{
    private:
        uint64_t _room_id;
        room_statu _statu;
        int _player_count;
        uint64_t _white_id;
        uint64_t _black_id;
        user_table *_tb_user;
        online_manager *_online_user;
        std::vector<std::vector<int>> _board;


    private:
        bool five(int row,int col, int row_off,int col_off,int color){
            // row和col是下棋位置
            // row_off 和col_off 是偏移量也是方向
            int cnt = 1 ;
            int search_row = row+row_off;
            int search_col = col+col_off;
            while(search_row < BOARD_ROW &&  search_row>=0&&
                search_col < BOARD_COL && search_col>= 0&& 
                _board[search_row][search_col] == color){
                cnt++;
                search_row += row_off;
                search_col += col_off;
             }
            search_row = row-row_off;
            search_col = col-col_off;
            while(search_row < BOARD_ROW &&  search_row>=0&&
                search_col < BOARD_COL && search_col>= 0&& 
                _board[search_row][search_col] == color){
                cnt++;
                search_row -= row_off;
                search_col -= col_off;
             }
             return (cnt>=5);
        }
        uint64_t check_win(int row, int col, int color)
        {
            if(five(row,col,0,1,color)||
                five(row,col,1,0,color)||
                five(row,col,-1,1,color)||
                five(row,col,-1,-1,color)){
                return color == WHITE_CHESS? _white_id: _black_id; 
            }
            return 0;
        }

    public:
        room(uint64_t room_id,user_table *tb_user,online_manager*online_user):
        _room_id(room_id),_statu(GAME_START),_player_count(0),
        _tb_user(tb_user),_online_user(online_user),
        _board(BOARD_ROW,std::vector<int>(BOARD_COL,0)){
            DLOG("%lu ROOM CREATE SUCCESS",_room_id);
        }
        ~room(){
            DLOG("%lu ROOM DELETE SUCCESS",_room_id);
        }
        uint64_t id(){return _room_id;}
        room_statu statu(){return _statu;}
        int player_count (){ return _player_count;}
        void add_white_user(uint64_t uid) {_white_id = uid; _player_count++;}
        void add_black_user(uint64_t uid) {_black_id = uid; _player_count++;}

        uint64_t get_white_user() { return _white_id;}
        uint64_t get_black_user() {return _black_id;}



        // 处理下棋动作
        Json::Value handler_chess(Json::Value &req){
            Json::Value json_resp = req;

            //2. 判断房间中两个玩家是否都在线，任意一个不在线就是另一方胜利

            uint64_t cur_uid = req["uid"].asUInt64();
            int chess_row = req["row"].asInt();
            int chess_col = req["col"].asInt();
            if(  _online_user->is_in_game_room(_white_id)  == false  ){
                json_resp["result"] = true;
                json_resp["reason"] = "对方掉线，不战而胜！！";
                json_resp["winner"] = (Json::UInt64)_black_id;
                return json_resp;
            }

            if(  _online_user->is_in_game_room(_black_id)  == false  ){
                json_resp["result"] = true;
                json_resp["reason"] = "对方掉线，不战而胜！！";
                json_resp["winner"] = (Json::UInt64)_white_id;
                return json_resp;
            }


            //3. 获取走棋位置，判断当前走棋是否合理（位置是否已经被占用）

            if(_board[chess_row][chess_col] != 0){
                json_resp["result"] = false;
                json_resp["reason"] = "当前位置已被占用";
                return json_resp;
            }
            int cur_color = cur_uid==_white_id ? WHITE_CHESS:BLACK_CHESS;
            _board[chess_row][chess_col] = cur_color;


            //4. 判断是否有玩家胜利（从当前走棋位置判断是否存在五星连珠）

            uint64_t winner_id = check_win(chess_row,chess_col,cur_color);
            if(winner_id != 0)
            {
                json_resp["reason"] = "恭喜你获得胜利！";
            }
            json_resp["result"] = true;
            json_resp["winner"] = (Json::UInt64)winner_id;
            return json_resp;
        }

        Json::Value handle_chat(Json::Value &req){
            // 检测房间是否一致
            
            Json::Value json_resp = req;
            // 消息中是否包含敏感词
            std::string msg = req["message"].asString();
            size_t pos = msg.find("垃圾");
            if(pos != std::string::npos){
                json_resp["result"] = false;
                json_resp["reason"] = "消息中包含敏感词，不能发送！！";
                return json_resp;
            }
            // 广播消息
            json_resp["result"] = true;
            return json_resp;
        }

        void handle_exit(uint64_t uid){
            Json::Value json_resp;
            if(_statu == GAME_START){
                uint64_t winner_id =(Json::UInt64)(uid==_white_id? _black_id:_white_id);
                json_resp["optype"] = "put_chess";
                json_resp["room_id"] = (Json::UInt64)_room_id;
                json_resp["uid"] = (Json::UInt64)uid;
                json_resp["row"] = -1;
                json_resp["col"] = -1;
                json_resp["result"] = true;
                json_resp["reason"] = "对方掉线，获得胜利";
                json_resp["winner"] = (Json::UInt64)winner_id;

                uint64_t loser_id = winner_id == _white_id? _black_id: _white_id;
                _tb_user->win(winner_id);
                _tb_user->lose(loser_id);
                _statu = GAME_OVER;                
                broadcast(json_resp);
            }
            _player_count--;
            return;
        }


        void handler_request(Json::Value &req){
            // 1. 校验房间号是否匹配
            Json::Value json_resp;
            uint64_t room_id = req["room_id"].asUInt64();
            if(room_id != _room_id){
                json_resp["optype"] = req["optype"].asString();
                json_resp["result"] = false;
                json_resp["reason"] = "房间号不匹配！！";
                
                return broadcast(json_resp);
            }
            // 2. 根据不同的请求类型调用不同的处理函数
            if(req["optype"].asString() == "put_chess")
            {
                json_resp = handler_chess(req);
                if(json_resp["winner"].asUInt64() != 0)
                {
                    uint64_t winner_id = json_resp["winner"].asUInt64();
                    uint64_t loser_id = winner_id == _white_id? _black_id: _white_id;
                    _tb_user->win(winner_id);
                    _tb_user->lose(loser_id);
                    _statu = GAME_OVER;

                }
            }else if(req["optype"].asString() == "chat")
            {
                json_resp = handle_chat(req);

                
            }
            else{
                json_resp["optype"] = req["optype"].asString();
                json_resp["result"] = false;
                json_resp["reason"] = "未知请求类型";
            }
            std::string body;
            json_util::serialize(json_resp,body);
            DLOG("房间-广播动作 : %s",body.c_str());
            return broadcast(json_resp);
        }

        void broadcast(Json::Value &rsp){
            // 1. 对要相应的信息进行序列化，将Json::Value 中的数据序列化成为json格式字符串
            std::string body;
            json_util::serialize(rsp,body);
            // 2. 获取房间中所有用户的通信连接
            // 3. 发送响应信息
            server_t::connection_ptr wconn =  _online_user->get_conn_from_room(_white_id);
            if(wconn.get() != nullptr){
                wconn->send(body);
            }else{
                DLOG("房间-白棋玩家连接获取失败");
            }
            server_t::connection_ptr bconn =   _online_user->get_conn_from_room(_black_id);
            if(bconn.get() != nullptr){
                bconn->send(body);
            }else{
                DLOG("房间-黑棋玩家连接获取失败");
            }

            return;
        }


};


using room_ptr = std::shared_ptr<room>;
class room_manager{
private:
    uint64_t _next_rid;
    user_table * _tb_user;
    online_manager* _online_user;
    std::mutex _mutex;
    std::unordered_map<uint64_t, room_ptr> _rooms;
    std::unordered_map<uint64_t, uint64_t> _users;
public:
    room_manager(user_table* ut, online_manager* om)
    :_next_rid(1),_tb_user(ut),_online_user(om)
    {
        DLOG("房间管理模块初始化完毕！");
    }
    ~room_manager(){DLOG("房间管理模块即将销毁");}
    room_ptr create_room(uint64_t uid1,uint64_t uid2)
    {
        //1. 校验两个用户是否都在游戏大厅中，只有都在才需要创建房间
        if( _online_user->is_in_game_hall(uid1) == false )
        {
            DLOG("用户： %lu 不在大厅中 ,创建房间失败", uid1);
            return room_ptr();
        } 
        if( _online_user->is_in_game_hall(uid2) == false )
        {
            DLOG("用户： %lu 不在大厅中 ,创建房间失败", uid2);
            return room_ptr();
        } 


        //2. 创建房间，将用户信息添加到房间中
        std::unique_lock<std::mutex> lock(_mutex);
        room_ptr rp(new room(_next_rid,_tb_user,_online_user));
        rp->add_black_user(uid1);
        rp->add_white_user(uid2);


        //3. 将房间信息管理起来
        _rooms.insert(std::make_pair(_next_rid,rp));
        _users.insert(std::make_pair(uid1,_next_rid));
        _users.insert(std::make_pair(uid2,_next_rid));

        _next_rid++;
        //4. 返回房间信息
        return rp;

    }

    room_ptr get_room_by_rid(uint64_t rid){
        
        std::unique_lock<std::mutex> lock(_mutex);

        auto it = _rooms.find(rid);
        if(it == _rooms.end()){
            return room_ptr();
        }
        return it->second;
    }

    room_ptr get_room_uid(uint64_t uid){
        std::unique_lock<std::mutex> lock(_mutex);
        
        auto uit = _users.find(uid);
        if(uit == _users.end()){
            return room_ptr();
        }
        uint64_t rid = uit->second;

        auto rit = _rooms.find(rid);
        if(rit == _rooms.end()){
            return room_ptr();
        }

        return rit->second;
    }

    void remove_room(uint64_t rid){
        // 通过房间id，获取房间信息
        room_ptr rp = get_room_by_rid(rid);
        if(rp.get() == nullptr){
            return;
        }
        //通过房间信息获取用户id
        uint64_t uid1 = rp->get_black_user();
        uint64_t uid2 = rp->get_white_user();
        //移除房间管理中的信息用户

        std::unique_lock<std::mutex> lock(_mutex);
        _users.erase(uid1);
        _users.erase(uid2);

        _rooms.erase(rid);
    }

    void remove_room_user(uint64_t uid){
        room_ptr rp = get_room_uid(uid);
        if(rp.get() == nullptr){
            return;
        }

        rp->handle_exit(uid);

        if(rp->player_count() == 0){
            remove_room(rp->id());
        }
        return ;
    }







};



#endif