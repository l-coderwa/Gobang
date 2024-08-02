#ifndef __M_SRV_H__
#define __M_SRV_H__
#include"db.hpp"
#include"matcher.hpp"
#include"online.hpp"
#include"room.hpp"
#include"session.hpp"
#include"util.hpp"

#define WWWROOT "./wwwroot/"
class gobang_server{
    private:
        server_t _svr;
        std::string _web_root;        
        user_table _ut;
        online_manager _om;
        room_manager _rm;
        matcher _mm;
        session_manager _sm;


    private:

        void file_handler(server_t::connection_ptr &conn)
        {
            // 静态资源请求处理

            //1. 获取到请求uri资源路径，了解客户端请求的页面文件名称
            websocketpp::http::parser::request req = conn->get_request();
            std::string uri = req.get_uri();
            //2. 组合出文件的实际路径
            std::string realpath = _web_root + uri;
            //3. 如果请求的是个目录，增加一个后缀 login。html
            if(realpath.back() == '/'){
                realpath += "login.html";
            }
            //4. 读取文件内容
            // 文件不存在 读取文件内容失败  返回404
            std::string body;
            bool ret = file_util::read(realpath,body);
            if(ret == false){
                body+="<html>";
                body+="<head>";
                body+="<meta charset= 'UTF-8'/>";
                body+="</head>";
                body+="<body>";
                body+= "<h1> Not Fount </h1>";
                body+="</body>";
                conn->set_status(websocketpp::http::status_code::not_found);
                conn->set_body(body);
                return ;

            }
            //5. 设置响应正文
            conn->set_body(body);
                conn->set_status(websocketpp::http::status_code::ok);

        }
        void http_resp(server_t::connection_ptr &conn,bool result, websocketpp::http::status_code::value code,
        const std::string reason){
                Json::Value json_resp;
                json_resp["result"] = result;
                json_resp["reason"] = reason;
                std::string resp_body;
                json_util::serialize(json_resp,resp_body);
                conn->set_status(code);
                conn->set_body(resp_body);
                conn->append_header("Content-Type","application/json");
                return ;
        }
        void reg(server_t::connection_ptr &conn)
        {
            // 用户注册功能请求的处理
            websocketpp::http::parser::request req = conn->get_request();
            //1. 获取到请求正文
            std::string req_body = conn->get_request_body();
            //2. 对正文进行反序列化，得到用户名和密码
            Json::Value login_info;
            Json::Value json_resp;

            bool ret = json_util::unserialize(req_body,login_info);
            if(ret == false){
                DLOG("反序列化注册信息失败");
                return http_resp(conn,false,websocketpp::http::status_code::bad_request,"请求的正文格式错误");
            }

            //3. 进行数据库的用户新增操作
            if(login_info["username"].isNull() || login_info["password"].isNull())
            {
                DLOG("用户名密码不完整");
                return http_resp(conn,false,websocketpp::http::status_code::bad_request,"请输入用户名或密码");

            }
            ret = _ut.insert(login_info);
            if(ret ==false){
                DLOG("插入数据失败");

                return http_resp(conn,false,websocketpp::http::status_code::bad_request,"用户名已经被占用");
            }

            //4. 如果成功了则返回200
            json_resp["result"] = true;
            json_resp["reason"] = "注册用户成功";
            std::string resp_body;
            json_util::serialize(json_resp,resp_body);
            conn->set_status(websocketpp::http::status_code::ok);
            conn->set_body(resp_body);
            conn->append_header("Content-Type","application/json");
        }

        void login(server_t::connection_ptr &conn)
        {
            //用户登录功能请求的处理

            //1. 获取请求正文，并进行json反序列化得到用户名和密码
            std::string req_body = conn->get_request_body();
            Json::Value login_info;
            bool ret = json_util::unserialize(req_body,login_info);
            if(ret == false){
                DLOG("反序列化登录信息失败");
                return http_resp(conn,false,websocketpp::http::status_code::bad_request,"请求的正文格式错误");
            }
            //2. 校验正文的完整性，进行数据库的用户信息验证
            if(login_info["username"].isNull() || login_info["password"].isNull())
            {
                DLOG("用户名密码不完整");
                return http_resp(conn,false,websocketpp::http::status_code::bad_request,"请输入用户名或密码");

            }
            ret = _ut.login(login_info);
            if(ret == false){
                DLOG("用户密码错误");
                return http_resp(conn,false,websocketpp::http::status_code::bad_request,"用户密码错误");
            }
            //3. 如果验证成功，给客户端创建session
            uint64_t uid = login_info["id"].asUInt64();
            session_ptr ssp = _sm.create_session(uid,LOGIN);
            if(ssp.get() == nullptr){
                DLOG("创建会话失败");
                return http_resp(conn,false,websocketpp::http::status_code::internal_server_error,"创建会话失败");

            }
            _sm.set_session_expire_time(ssp->ssid(),30000);
            //4. 设置响应头部： Set-Cookie 
            std::string cookie_ssid = "SSID="+ std::to_string(ssp->ssid());
            conn->append_header("Set-Cookie",cookie_ssid);
            return http_resp(conn,true,websocketpp::http::status_code::ok
            ,"登陆成功！");
        }


        bool get_cookie_val(const std::string &cookie_str, const std::string &key,std::string &val){
            std::string sep = "; ";
            std::vector<std::string> cookie_arr;
            string_util::split(cookie_str,sep,cookie_arr);
            for(auto str: cookie_arr){
                std::vector<std::string> tmp_arr;
                string_util::split(str,"=",tmp_arr);
                if(tmp_arr.size()!= 2){continue;}
                if(tmp_arr[0] == key){
                    val = tmp_arr[1];
                    return true;
                }
            }
            return false;
        }


        void info(server_t::connection_ptr &conn)
        {
            Json::Value err_resp;
            // 用户信息获取功能请求的处理
            std::string cookie_str = conn->get_request_header("Cookie");
            if(cookie_str.empty()){
                return http_resp(conn,true,websocketpp::http::status_code::bad_request,"登陆过期，请重新登陆");
            }
            // 1. 获取请求信息中的cookie，从cookie中获取ssid
            std::string ssid_str; 
            bool ret = get_cookie_val(cookie_str,"SSID",ssid_str); 
            if(ret == false){
                return http_resp(conn,true,websocketpp::http::status_code::bad_request,"找不到ssid信息，请重新登录");
            }
            //2. 在session管理中查找对应的会话信息
            session_ptr ssp = _sm.get_session_by_ssid(std::stol(ssid_str));
            //  没有找到session，则认为登陆已经过期需要重新登陆
            if(ssp.get() == nullptr){
                return http_resp(conn,true,websocketpp::http::status_code::bad_request,"找不到用户信息，请重新登录");
            }
            //3. 从数据库中取出用户信息，进行序列化发送给客户端
            Json::Value user_info;
            uint64_t uid = ssp->get_user();
            ret = _ut.select_by_id(uid,user_info);
            if(ret == false){
                return http_resp(conn,true,websocketpp::http::status_code::bad_request,"找不到用户信息，请重新登陆");
            }
            std::string body;
            json_util::serialize(user_info,body);
            conn->set_body(body);
            conn->append_header("Content-Type","application/json");
            conn->set_status(websocketpp::http::status_code::ok);
            //4. 刷新session的过期时间
            _sm.set_session_expire_time(ssp->ssid(),SESSION_TIMEOUT);            
        }

        void http_callback(websocketpp::connection_hdl hdl){

            server_t::connection_ptr conn = _svr.get_con_from_hdl(hdl);
            websocketpp::http::parser::request req = conn->get_request();
            std::string method = req.get_method();
            std::string uri =  req.get_uri();
            if(method == "POST" && uri == "/reg"){
                return reg(conn);
            }else if(method == "POST" && uri == "/login"){
                return login(conn);
            }else if(method == "GET" && uri == "/info"){
                return info(conn);
            }else{
                return file_handler(conn);
            }
        }


        void ws_resp( server_t::connection_ptr conn,Json::Value&resp)
        {
            std::string body;
            json_util::serialize(resp,body);
            conn->send(body);
        }

        session_ptr get_session_by_cookie(server_t::connection_ptr conn)
        {
            Json::Value err_resp;
            // 用户信息获取功能请求的处理
            std::string cookie_str = conn->get_request_header("Cookie");
            if(cookie_str.empty()){
                err_resp["optype"] = "hall_ready";
                err_resp["reason"] = "没有找到cookie信息，需要重新登陆";
                err_resp["result"] = false;
                ws_resp(conn,err_resp);
                return session_ptr();
            }
            // 1. 获取请求信息中的cookie，从cookie中获取ssid
            std::string ssid_str; 
            bool ret = get_cookie_val(cookie_str,"SSID",ssid_str); 
            if(ret == false){
                err_resp["optype"] = "hall_ready";
                err_resp["reason"] = "找不到ssid信息，需要重新登陆";
                err_resp["result"] = false;
                ws_resp(conn,err_resp);
                return session_ptr();
      }
            //2. 在session管理中查找对应的会话信息
            session_ptr ssp = _sm.get_session_by_ssid(std::stol(ssid_str));
            //  没有找到session，则认为登陆已经过期需要重新登陆
            if(ssp.get() == nullptr){
                err_resp["optype"] = "hall_ready";
                err_resp["reason"] = "找不到session信息，需要重新登陆";
                err_resp["result"] = false;
                ws_resp(conn,err_resp);
                return session_ptr();

            }
            return ssp;
        }

        void wsopen_game_hall( server_t::connection_ptr conn){
            Json::Value resp_json;
            session_ptr ssp = get_session_by_cookie(conn);
            if(ssp.get() == nullptr){
                return ;
            }
            if(_om.is_in_game_hall(ssp->get_user()) || 
            _om.is_in_game_room(ssp->get_user())  ){
                resp_json["optype"] = "hall_ready";
                resp_json["reason"] = "玩家重复登陆";
                resp_json["result"] = false;
                return ws_resp(conn,resp_json);

            }

            _om.enter_game_hall(ssp->get_user(), conn);


            resp_json["optype"] = "hall_ready";
            resp_json["result"] = true;
            ws_resp(conn,resp_json);
            _sm.set_session_expire_time(ssp->ssid(),SESSION_FOREVER);
        }



        void wsopen_game_room(server_t::connection_ptr conn){

            //1. 获取当前客户端的session
            session_ptr ssp = get_session_by_cookie(conn);
            if(ssp.get() == nullptr){
                return ;
            }    
            //2. 当前用户是否已经在游戏房间或者游戏大厅中
            Json::Value resp_json;
            if(_om.is_in_game_hall(ssp->get_user()) || 
            _om.is_in_game_room(ssp->get_user())  ){
                resp_json["optype"] = "room_ready";
                resp_json["reason"] = "玩家重复登陆";
                resp_json["result"] = false;
                return ws_resp(conn,resp_json);
            }
            //3. 判断当前用户是否已经创建好了房间
            room_ptr rp = _rm.get_room_uid(ssp->get_user());
            if(rp.get() == nullptr){
                resp_json["optype"] = "room_ready";
                resp_json["reason"] = "没有找到玩家的房间信息";
                resp_json["result"] = false;
                return ws_resp(conn,resp_json);
            }
            //4. 将当前用户添加到在线用户管理的游戏房间中
            _om.enter_game_room(ssp->get_user(),conn);
            //5. 将session重新设置为永久存在
            _sm.set_session_expire_time(ssp->ssid(),SESSION_FOREVER);

            resp_json["optype"] = "room_ready";
            resp_json["result"] = true;
            resp_json["room_id"] = (Json::UInt64)rp->id();
            resp_json["uid"] = (Json::UInt64)ssp->get_user();
            resp_json["white_id"] =(Json::UInt64) rp->get_white_user();
            resp_json["black_id"] = (Json::UInt64)rp->get_black_user();

            return ws_resp(conn,resp_json);
        }



        void wsopen_callback(websocketpp::connection_hdl hdl){
            server_t::connection_ptr conn = _svr.get_con_from_hdl(hdl);
            websocketpp::http::parser::request req = conn->get_request();
            std::string uri =  req.get_uri();
            if(uri == "/hall"){
                return wsopen_game_hall(conn);
            }else if(uri == "/room"){
                return wsopen_game_room(conn);

            }
        }
        void wsclose_game_hall(server_t::connection_ptr conn){
            session_ptr ssp = get_session_by_cookie(conn);
            if(ssp.get() == nullptr){
                return ;
            }            
            _om.exit_game_hall(ssp->get_user());
            _sm.set_session_expire_time(ssp->ssid(),SESSION_TIMEOUT);
        }


        void wsclose_game_room(server_t::connection_ptr conn)
        {
            session_ptr ssp = get_session_by_cookie(conn);
            if(ssp.get() == nullptr){
                return ;
            }       
            _om.exit_game_room(ssp->get_user());
            _sm.set_session_expire_time(ssp->ssid(),SESSION_TIMEOUT);
            _rm.remove_room_user(ssp->get_user());
        }


        void wsclose_callback(websocketpp::connection_hdl hdl){
            server_t::connection_ptr conn = _svr.get_con_from_hdl(hdl);
            websocketpp::http::parser::request req = conn->get_request();
            std::string uri =  req.get_uri();
            if(uri == "/hall"){
                return wsclose_game_hall(conn);
            }else if(uri == "/room"){
                return wsclose_game_room(conn);
            }
        }

        void wsmsg_game_hall(server_t::connection_ptr conn, server_t::message_ptr msg){
            //1. 身份验证
            Json::Value resp_json;
            std::string resp_body;
            session_ptr ssp = get_session_by_cookie(conn);
            if(ssp.get() == nullptr){
                return ;
            }  
            // 获取请求信息
            std::string req_body = msg->get_payload();
            Json::Value req_json;
            bool ret = json_util::unserialize(req_body,req_json);
            if(ret == false){
                resp_json["result"]=false;
                resp_json["reason"] = "请求信息解析失败";
                return ws_resp(conn,resp_json);
            }
            //2. 对于请求进行处理
            // 对战匹配
            if(!req_json["optype"].isNull() &&
            req_json["optype"].asString()=="match_start")
            {
                _mm.add(ssp->get_user());
                resp_json["optype"] = "match_start";                
                resp_json["result"]=true;
                return ws_resp(conn,resp_json);
            }
            else if(!req_json["optype"].isNull() &&
            req_json["optype"].asString()=="match_stop"){
                _mm.del(ssp->get_user());
                resp_json["optype"] = "match_stop";                
                resp_json["result"]=true;
                return ws_resp(conn,resp_json);
            }
            else{
                resp_json["optype"] = "unknow";  
                resp_json["reason"] = "请求类型未知";                

                resp_json["result"]=false;
                return ws_resp(conn,resp_json);               
            }
        }


        void wsmsg_game_room(server_t::connection_ptr conn, server_t::message_ptr msg){
            Json::Value resp_json;
            session_ptr ssp = get_session_by_cookie(conn);
            if(ssp.get() == nullptr){
                DLOG("房间-没有找到会话信息");
                return ;
            }  

            room_ptr rp = _rm.get_room_uid(ssp->get_user());
            if(rp.get() == nullptr){
                resp_json["optype"] = "unknow";
                resp_json["reason"] = "没有找到玩家的房间信息";
                resp_json["result"] = false;
                DLOG("房间-没有找到玩家房间信息");
                return ws_resp(conn,resp_json);
            }
            Json::Value req_json;
            std::string req_body = msg->get_payload();
            bool ret = json_util::unserialize(req_body,req_json);
            if(ret == false){
                resp_json["optype"] = "unknow";
                resp_json["reason"] = "请求解析失败";
                resp_json["result"] = false;
                DLOG("房间-反序列化请求失败");
                return ws_resp(conn,resp_json);
            }
            return rp->handler_request(req_json);
        }

        
        void wsmsg_callback(websocketpp::connection_hdl hdl, server_t::message_ptr msg){
            server_t::connection_ptr conn = _svr.get_con_from_hdl(hdl);
            websocketpp::http::parser::request req = conn->get_request();
            std::string uri =  req.get_uri();
            if(uri == "/hall"){
                return wsmsg_game_hall(conn,msg);
            }else if(uri == "/room"){
                return wsmsg_game_room(conn,msg);
            }
        }

    public:
        gobang_server(const std::string &host,
        const std::string &username, 
        const std::string & passwd, 
        const std::string &db, 
        uint16_t port=3306,
        const std::string &wwwroot = WWWROOT):
        _web_root(wwwroot),_ut(host,username,passwd,db,port),
        _rm(&_ut,&_om),_sm(&_svr),_mm(&_rm,&_ut,&_om){
            _svr.set_access_channels(websocketpp::log::alevel::none);
            _svr.init_asio();
            _svr.set_reuse_addr(true);
            _svr.set_http_handler(std::bind(&gobang_server::http_callback,this,std::placeholders::_1));
            _svr.set_open_handler(std::bind(&gobang_server::wsopen_callback,this,std::placeholders::_1));
            _svr.set_close_handler(std::bind(&gobang_server::wsclose_callback,this,std::placeholders::_1));
            _svr.set_message_handler(std::bind(&gobang_server::wsmsg_callback,this,std::placeholders::_1,std::placeholders::_2));
        }

        void start(int port){
            //监听端口
            _svr.listen(port);

            // 接受tcp链接
            _svr.start_accept();

            // 开始运行服务器
            _svr.run();
        }
};


#endif