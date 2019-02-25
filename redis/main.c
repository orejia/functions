#include <stdio.h>
#include <hiredis/hiredis.h>

int main()
{
	//连接远程6379实例
	redisContext *conn = redisConnect("192.168.2.131", 6379);
	if (conn->err != 0)
	{
		printf("connection error: %s\n", conn->errstr);
		return -1;
	}

	//执行redis命令(处理数组返回值)
	redisReply *reply = (redisReply *)redisCommand(conn, "keys *");
	if (reply->type == REDIS_REPLY_ARRAY)
	{
		for (size_t i = 0; i < reply->elements; i++)
		{
			redisReply *reply2 = reply->element[i];
		  if (reply2->type == REDIS_REPLY_STRING)
		  {
		  	printf("%s\n", reply2->str);
		  }
		}
	}
	//释放redisReply对象资源
	freeReplyObject(reply);
	reply = NULL;

	//执行redis命令(处理字符串返回值)
	reply = (redisReply *)redisCommand(conn, "get hello");
	if (reply->type == REDIS_REPLY_STRING)
	{
		 printf("%s\n", reply->str);
	}
	freeReplyObject(reply);
	reply = NULL;


	//关闭连接
	redisFree(conn);
	return 0;
}
