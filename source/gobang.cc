#include"logger.hpp"
#include"util.hpp"
#include"db.hpp"
#include"online.hpp"
#include"room.hpp"
#include"session.hpp"
#include"matcher.hpp"
#include"server.hpp"


#define HOST "127.0.0.1"
#define USER "root"
#define PASSWD "abcd1234"
#define DBNAME "gobang"
#define PORT 3306

void mysql_test()
{
    MYSQL* mysql = mysql_util::mysql_create(HOST,USER,PASSWD,DBNAME,PORT);
    const char* sql = "insert stu values(null,'小He',18,53,68,87);";
    bool ret = mysql_util::mysql_exec(mysql,sql);
    if(ret == false)
    {
        return;
    }
    mysql_util::mysql_destroy(mysql);
}

void json_test()
{
    Json::Value root;
    std::string body;
    root["姓名"] = "小明";
    root["年龄"] = 18;
    root["成绩"].append(98);
    root["成绩"].append(88);
    root["成绩"].append(78.5);

    json_util::serialize(root,body);
    DLOG("%s",body.c_str());

    Json::Value val;
    json_util::unserialize(body,val);

    std::cout<< "姓名: " << val["姓名"].asString() << std::endl;
    std::cout<< "年龄: " << val["年龄"].asInt() << std::endl;
    int size = val["成绩"].size();
    for(int i = 0;i<size;i++)
    {
        std::cout<<"成绩: "<<  val["成绩"][i].asFloat()<<std::endl;
    }
}

void split_test()
{
    std::string str = "123,234,,,,,,345";
    std::vector<std::string> array;
    string_util::split(str,",",array);
    for(auto e : array)
    {
        DLOG("%s",e.c_str());
    }
}

void file_test()
{
    std::string filename = "./Makefile";
    std::string body;
    file_util::read(filename,body);
    DLOG("%s",body.c_str());
}

void db_test(){
    user_table ut(HOST,USER,PASSWD,DBNAME,PORT);
    Json::Value user;
    // user["username"] = "小明";
    // user["password"] = "123456";
    bool ret = ut.lose(1);
    // std::string body;
    // json_util::serialize(user,body);
    // std::cout<< body<<std::endl;
    // if(ret == false){
    //     DLOG("LOGIN FAILED");
    // }

}

void online_test(){
    online_manager om;
    server_t::connection_ptr conn;
    uint64_t uid = 2 ;
    om.enter_game_hall(uid,conn);
    if(om.is_in_game_hall(uid))
    {
        DLOG("IN GAME HALL");
    }else{
        DLOG("NOT IN GAME HALL");
    }
    om.exit_game_hall(uid);
    if(om.is_in_game_hall(uid))
    {
        DLOG("EXIT GAME HALL");
    }else{
        DLOG("NOT IN GAME HALL");
    }
}

int main()
{
    //fprintf(stdout,"%s-%d\n","bite",100);
    // ILOG("biteI");
    // DLOG("biteD");
    // ELOG("biteE");
    // json_test();
    // split_test();
    // file_test();
    // db_test();
    // online_test();
    // user_table ut(HOST,USER,PASSWD,DBNAME,PORT);
    // online_manager om;
    // // room r(10,&ut,&om);
    // room_manager rm(&ut,&om);
    // room_ptr rp = rm.create_room(10,20);
    // matcher mc(&rm,&ut,&om);

    gobang_server _server(HOST,USER,PASSWD,DBNAME,PORT);
    _server.start(8080);

    return 0;
}
