#include <libxml/parser.h>
#include <libxml/tree.h>

static void
/*递归调用打印子节点的值*/
print_element_names(xmlNode * a_node)
{
    xmlNode *cur_node = NULL;

    for (cur_node = a_node; cur_node; cur_node = cur_node->next) {
        if (cur_node->type == XML_ELEMENT_NODE) {
            printf("node type: Element, name: %s\n", cur_node->name);

        }

        print_element_names(cur_node->children);
    }
}

int main(int argc, char* argv[])
{
    xmlDocPtr doc; //定义解析文档指针
    xmlNodePtr curNode; //定义结点指针(你需要它为了在各个结点间移动)
    xmlChar *szKey; //临时字符串变量
    char *szDocName;
    
    if (argc <= 1) 
    {
       printf("Usage: %s docname/n", argv[0]);
       return(0);
    }
    szDocName = argv[1];
    //无空格输出
    doc = xmlReadFile(szDocName,NULL,XML_PARSE_NOBLANKS); //解析文件
    //doc = xmlReadFile(szDocName,"GB2312",XML_PARSE_RECOVER); //解析文件
    if (NULL == doc)
    { 
       printf("Document not parsed successfully/n"); 
       return -1;
    }
    curNode = xmlDocGetRootElement(doc); //确定文档根元素
    if (NULL == curNode)
    {
       printf("empty document/n");
       xmlFreeDoc(doc);
       return -1;
    }

    print_element_names(curNode);

    /*
    //if (xmlStrcmp(curNode->name, BAD_CAST "root"))
    if (!xmlStrcmp(curNode->name, (const xmlChar*) "root"))
    {

       xmlFreeDoc(doc);
       return 0;
    }
    curNode = curNode->children;
    while(curNode != NULL)
    {
       //取出节点中的内容
       if (!xmlStrcmp(curNode->name, (const xmlChar*) "msg"))
            printf("msg: %s\n",((char*)XML_GET_CONTENT(curNode->children)));
       //szKey = xmlNodeGetContent(curNode);
       //printf("[%s][%s]", curNode->name,szKey);
       curNode = curNode->next;
     }
   */

     xmlFreeDoc(doc);
    return 0; 

}
