#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "queries.h"
#include "intermediate.h"

int parseQueries(char* queryFileName, relationInfo* relInfo, int relationNum, JobScheduler* sch){
    char* line=NULL;
    size_t len = 0;
    ssize_t read;
    char *token;
    char *endCheck;
    FILE *fp;


    char* relations;
    char* predicates;
    char* projections;

    //printf("Start of parsing the query file %s...\n", queryFileName);

    // printf("Value in r10, column 2, row 191: %d\n", relInfo[10].columns[1][190]);
    fp = fopen(queryFileName, "r");
    if(fp == NULL){
        exit(EXIT_FAILURE);
    }

    int query_counter=0;
    resultQ *q=initializeQ();
    int capacity=20;
    // queryThreadArgs** args = (queryThreadArgs**)malloc(capacity * sizeof(queryThreadArgs*));
    argsList* list = initializeArgsList();
    while((read = getline(&line, &len, fp)) != -1){
        int isValid = 1;        //used to determine if the query contains formatting errors

        char* newline = strchr(line, '\n');
        if (newline) *newline = '\0';

        if(strcmp(line,"F") == 0){
            continue;
        }
        query_counter++;



        addToArgsList(list, sch, q, line, relInfo, query_counter);

        queryThreadArgs* current = list->head;
        Job* j=createJob((void*)queryFun,current);

        submit_job(sch,j);
        // if(query_counter==1) break;
    }

    // freein memory from args


    sch->isFinished=1;
    int value=0; 
    while(1){
        sem_getvalue(&q->full,&value);
        if(value==query_counter)
            break;
    }

    for(int i=0 ; i<sch->numOfThreads ; i++)
        sem_post(&queue_full);
    sem_getvalue(&queue_full,&value);
    for(int i=0 ; i<sch->numOfThreads ; i++){

        pthread_join(sch->thread_ids[i],NULL);
    }




    freeArgsList(list);


    while(query_counter){
        sem_wait(&q->full);     //wait until there is something in the queue



        sem_wait(&q->lock);
        Element e=pop(q);

        sem_post(&q->lock);
        printf("%s\n",e.data);

        query_counter--;
    }

    sem_destroy(&q->lock);
    sem_destroy(&q->full);
    free(q->heap);
    free(q);

    printf("all done with query handling\n");

    fclose(fp);
    if (line){
        free(line);
    }

}


int isFilter(char* str){            //      if str is filter returns 1

    int flag=0;
    for ( int i=0 ; str[i] ; i++)
    {
        if(str[i]=='<' || str[i]=='>' || str[i]=='=')
            flag = 1;

        if (flag)
            if (str[i]=='.')
                return 0;
    }

    return 1;
}

int sameRel(char* predicate){              /*      an einai idio relation epistrefei 1 an einai idio relation kai idio column epistrefei 2 allios epistrefei 0     */

    int rel1=0,rel2=0,dot=0,op=0,col1=0,col2=0;
    int i=0;

    while(predicate[i]){
        if (predicate[i]=='.'){
            dot++;
            i++;
            continue;
        }
        if (predicate[i]=='=' || predicate[i]=='<' || predicate[i]=='>'){
            op++;
            i++;
            continue;
        }
        if(dot==0 && predicate==0)
            rel1=10*rel1+predicate[i]-'0';

        if (op==1 && dot==1)
            rel2=10*rel2+predicate[i]-'0';
        if (dot==1 && op==0)
            col1=10*col1+predicate[i]-'0';
        if (dot==2 && op==1)
            col2=10*col2+predicate[i]-'0';

        i++;
    }


    if (rel1==rel2 && col1==col2)
        return 2;

    return rel1==rel2;

}




relation* relColumn(relation* rel, int col){            /*      dexetai os orisma ena relation kai ena column kai epistrefei relation me mono auto to column        */

    relation* new=createEmptyRelation(rel->num_tuples);

    int data=0;
    relationPayloadList* temp;

    for(int i=0 ; i<rel->num_tuples ; i++){

        temp=rel->tuples[i].payloadList;

        for(int j=0; j<col ; j++){

            temp=temp->next;

        }

        addToPayloadList(new->tuples[i].payloadList,temp->data);

    }

    return new;

}

predicate* createPredicate(char* predicateStr, int order){
    predicate* newPredicate = malloc(sizeof(predicate));
    int isFilter = 0;
    int flag = 0;
    int filterFlag = 1;
    char tempPredicates[50];

    newPredicate->done=0;

    strcpy(tempPredicates, predicateStr);


    newPredicate->predicate=predicateStr;

    for ( int i=0 ; predicateStr[i] ; i++)
    {
        if(predicateStr[i]=='<' || predicateStr[i]=='>' || predicateStr[i]=='='){
           newPredicate->operation = predicateStr[i];
           flag = 1;
        }

        if (flag) {
            if (predicateStr[i] == '.') {
                filterFlag = 0;
                break;
            }
        }

    }
    if(filterFlag == 0){
        isFilter = 0;
        newPredicate->isFilter = isFilter;
    }else {
        isFilter = 1;
        newPredicate->isFilter = isFilter;
    }

    char* leftToken;
    char* rightToken;

    char *saveptr;
    leftToken = strtok_r(tempPredicates, "><=", &saveptr);
    newPredicate->leftRel=returnRelation(leftToken);
    rightToken = strtok_r(NULL, "\0", &saveptr);
    newPredicate->rightRel=returnRelation(rightToken);
    char* leftRelationStr = strtok_r(leftToken, ".", &saveptr);
    tuple* leftRelation = createTuple(atoi(leftRelationStr));
    char* leftRelationColumnStr = strtok_r(NULL, "\0", &saveptr);
    leftRelation->payloadList->data = atoi(leftRelationColumnStr);
    newPredicate->leftRelation = leftRelation;

    if(isFilter != 1){
        char* rightRelationStr = strtok_r(rightToken, ".", &saveptr);
        tuple* rightRelation = createTuple(atoi(rightRelationStr));
        char* rightRelationColumnStr = strtok_r(NULL, "\0", &saveptr);
        rightRelation->payloadList->data = atoi(rightRelationColumnStr);
        newPredicate->rightRelation = rightRelation;
    }else{
        newPredicate->rightRelation = NULL;
        newPredicate->value = atoi(rightToken);
    }
    newPredicate->order = order;

    return newPredicate;
}




int biggerRel(relation* rel1,relation* rel2){               /* rel1>rel2 return 0 else return 1*/


    if (rel1->num_tuples > rel2->num_tuples)    return 0;
    return 1;

}



int returnRelation(char *str){      // str is of type 0.1
    int rel=0;
    for (int i=0 ; str[i] ; i++){
        if (str[i]=='.')
            return rel;
        rel=10*rel + str[i]-'0';
    }

}










int returnColumn(char* predicate){

    int col=0;

    int flag=0;

    for (int i=0 ; predicate[i] ; i++){
        if (predicate[i]=='.'){
            flag=1;
        }
    col=10*col + predicate[i]-'0';
    }


    return col;
}


