#include <stdio.h>
#include <mysql/mysql.h>

int main()
{
	MYSQL * mysql = NULL ;
  MYSQL_RES * res = NULL;
	unsigned int fields = 0; //返回结果集列的数目
	MYSQL_ROW row = NULL;    //保存一行数据

  //1.初始化
	mysql = mysql_init(NULL);
	//2.连接
	mysql_real_connect(mysql, "47.106.235.56", "orejia",
	                   "orejia@1630", "mysql", 0, NULL, 0);

  //3.执行sql
	if (0 != mysql_query(mysql, "show databases"))
	{
		printf("sql exec error.\n");
		return -1;
	}

  //4.获取sql执行结果集
	res = mysql_store_result(mysql); 
	if (res == NULL && mysql_errno(mysql))
	{
	  printf("Failed to get results, Error: %s\n", 
		        mysql_error(mysql));
		  return -1;
	}

  //4.获取结果集列的数目
  fields = (unsigned int)mysql_num_fields(res);
	//while (row = mysql_fetch_row(res))
	while (1)
	{
	  //5.获取单行数据(每次调用自动返回下一行)
		row = mysql_fetch_row(res);
		if (row == NULL)
		{
			break;
		}

    //6.访问行内每一列数据
		unsigned int i;
		for (i = 0; i < fields; i++)
		{
			printf("%s ", row[i]);
		}
		printf("\n");
	}

  //7.释放结果集
	mysql_free_result(res);
	//8.关闭连接
	mysql_close(mysql);

  return 0;
}
