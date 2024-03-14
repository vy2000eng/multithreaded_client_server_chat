//
// Created by voldemort on 3/3/24.
//

#include "messaging.h"

int P2P_communication_thread(client_args * _client_args, int initiating_or_accepting)
{
    thread_t        thread_arr[2];
    thread_t        u_i_thread;
    void         *  receiving_thread_return_value;
    void         *  sending_thread_return_value;

    start_threads(_client_args,  initiating_or_accepting,thread_arr);

    if(pthread_create(&u_i_thread, NULL, user_input_thread, _client_args)!=0)
    {
        perror("Failed to create thread");
        return -1;
    }
    pthread_detach(u_i_thread);

    if(pthread_join(thread_arr[1], &receiving_thread_return_value)!= 0 )
    {
        printf("rec joined\n");
        return -1;
    }
    printf("on the outside of the thread recv");

    if(pthread_join(thread_arr[0], &sending_thread_return_value)!= 0 )
    {
        printf("send joined\n");
        return -1;
    }
    printf("on the outside of the thread send");
    free(_client_args);
    return 0;

}

int start_threads(client_args * _client_args, int initiating_or_accepting,thread_t *thread_arr)
{

    if(!initiating_or_accepting)
    {
        if(pthread_create(&thread_arr[0], NULL, handle_receiving, _client_args)!=0)
        {
            perror("Failed to create thread");
            return -1;
        }

        if(pthread_create(&thread_arr[1], NULL, handle_sending, _client_args)!=0)
        {
            perror("Failed to create thread");
            return -1;
        }
    }
    else
    {
        if(pthread_create(&thread_arr[1], NULL, handle_receiving, _client_args)!=0)
        {
            perror("Failed to create thread");
            return -1;
        }

        if(pthread_create(&thread_arr[0], NULL, handle_sending, _client_args)!=0)
        {
            perror("Failed to create thread");
            return -1;
        }
    }
    return 0;

}
void*user_input_thread(void * arg){
    client_args  * _client_args;
    mtx_lock(&communication_mutex);
    _client_args = (client_args*)arg;
    mtx_unlock(&communication_mutex);

    printf("TYPE YOU MSG AND PRESS ENTER\n");
    while(1)
    {
        sem_wait(&messaging_semaphore);
        mtx_lock(&communication_mutex);
        if(fgets(user_input, sizeof(user_input), stdin) != NULL) {
            user_input[strcspn(user_input, "\n")] = 0;
            mtx_unlock        (&communication_mutex);
            // Check input and set flags or shared variables
            mtx_lock       (&termination_mutex);
            if (strcmp     (user_input, "EXIT") == 0 || should_terminate)
            {
                printf("the client terminated the connection\n");

                should_terminate = 1;
               // close(*_client_args->connected_client_socket);
                mtx_unlock (&termination_mutex);
                sem_post   (&connection_semaphore) ;
                break;

            }
            mtx_unlock(&termination_mutex);
            sem_post  (&connection_semaphore);
        }
    }
    return NULL;

}

void * handle_sending(void * arg)
{
    client_args  * _client_args;
    message_packet _message_packet;
    int          * thread_return_value;

    mtx_lock(&communication_mutex);
    _client_args = (client_args*)arg;
    mtx_unlock(&communication_mutex);

    thread_return_value              = malloc(sizeof (int));
    _message_packet.packet_type.type = type_message_packet;

    while (1)
    {

        sem_wait        (&connection_semaphore);

        mtx_lock            (&termination_mutex);
        if (strcmp     (user_input, "EXIT") == 0 || should_terminate)
        {
            close(*_client_args->connected_client_socket);
            should_terminate = 1;
            printf("client terminated the connection via 'EXIT' cmd\n");
            mtx_unlock          (&termination_mutex);
            break;
        }

        // if(should_terminate){mtx_unlock(&termination_mutex); *thread_return_value =0;  break;}
        mtx_unlock          (&termination_mutex);


        mtx_lock        (&_mutex);
        memcpy          (_message_packet.message, user_input, sizeof(_message_packet.message));
        memset          (user_input, 0, sizeof(user_input));
        mtx_unlock      (&_mutex);

        if(send_packet(*_client_args->connected_client_socket, &_message_packet) < 0)
        {
            perror             ("send_packet(*_client_args->connected_client_socket, &_message_packet)\n");
            mtx_lock           (&termination_mutex);
            close              (*_client_args->connected_client_socket);
            should_terminate = 1;
            mtx_unlock         (&termination_mutex);
            close              (*_client_args->connected_client_socket);
            *thread_return_value = -1;
            break;
        }
        printf  ("msg sent: %s\n", _message_packet.message);
        memset  (&_message_packet, 0, sizeof (message_packet));
        sem_post(&messaging_semaphore);
    }
    pthread_exit(thread_return_value);
}

void * handle_receiving(void * arg)
{
    client_args  * _client_args;
    message_packet _message_packet;
    int          * thread_return_value;

    mtx_lock       (&communication_mutex);
    _client_args = (client_args*)arg;
    mtx_unlock     (&communication_mutex);

    memset                             (&_message_packet, 0, sizeof (message_packet));
    thread_return_value              = malloc(sizeof (int));
    _message_packet.packet_type.type = type_message_packet;

    while(1)
    {


        int n =receive_packet(*_client_args->connected_client_socket, &_message_packet);
        mtx_lock                (&termination_mutex);
        if (strcmp     (user_input, "EXIT") == 0 || should_terminate)
        {
            close(*_client_args->connected_client_socket);
            should_terminate = 1;
            printf("client terminated the connection via 'EXIT' cmd\n");
            mtx_unlock          (&termination_mutex);
            break;
        }
        //if(should_terminate)    {mtx_unlock(&termination_mutex); *thread_return_value =0; break;}
        mtx_unlock              (&termination_mutex);
        if(n<=0)
        {
            if(n == -1)
            {
                printf              ("the client disconnected");
                mtx_lock            (&termination_mutex);
                close               (*_client_args->connected_client_socket);
                should_terminate  = 1;
                mtx_unlock          (&termination_mutex);
                *thread_return_value = -1;
                break;
            }

            perror             ("receive_packet(*_client_args->connected_client_socket, &_message_packet\n");
            mtx_lock           (&termination_mutex);
            close              (*_client_args->connected_client_socket);
            should_terminate = 1;
            mtx_unlock         (&termination_mutex);
            *thread_return_value = -1;
            break;
        }
        if (strcmp     (_message_packet.message, "EXIT") == 0)
        {
            printf              ("the client disconnected via EXIT cmd");
            mtx_lock            (&termination_mutex);
            close               (*_client_args->connected_client_socket);
            should_terminate  = 1;
            mtx_unlock          (&termination_mutex);
            break;

        }
        sem_post(&messaging_semaphore);
        printf  ("msg received: %s\n",_message_packet.message);
        memset  (&_message_packet, 0, sizeof (message_packet));
    }
    pthread_exit(thread_return_value);
}







