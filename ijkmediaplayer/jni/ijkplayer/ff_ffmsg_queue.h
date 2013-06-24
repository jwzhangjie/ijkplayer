/*
 * ff_ffmsg_queue.h
 *      based on PacketQueue in ffplay.c
 *
 * Copyright (c) 2013 Zhang Rui <bbcallen@gmail.com>
 *
 * This file is part of ijkPlayer.
 *
 * ijkPlayer is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * ijkPlayer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with ijkPlayer; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef FFPLAY__FF_FFMSG_QUEUE_H
#define FFPLAY__FF_FFMSG_QUEUE_H

#include "ff_ffinc.h"
#include "ff_ffmsg.h"

typedef struct AVMessage {
    int what;
    int arg1;
    int arg2;
    struct AVMessage *next;
} AVMessage;

typedef struct MessageQueue {
    AVMessage *first_msg, *last_msg;
    int nb_messages;
    int abort_request;
    SDL_mutex *mutex;
    SDL_cond *cond;
} MessageQueue;

// TODO: msg pool
inline static int msg_queue_put_private(MessageQueue *q, AVMessage *msg)
{
    AVMessage *msg1;

    if (q->abort_request)
        return -1;

    msg1 = av_malloc(sizeof(AVMessage));
    if (!msg1)
        return -1;
    *msg1 = *msg;
    msg1->next = NULL;

    if (!q->last_msg)
        q->first_msg = msg1;
    else
        q->last_msg->next = msg1;
    q->last_msg = msg1;
    q->nb_messages++;
    SDL_CondSignal(q->cond);
    return 0;
}

inline static int msg_queue_put(MessageQueue *q, AVMessage *msg)
{
    int ret;

    SDL_LockMutex(q->mutex);
    ret = msg_queue_put_private(q, msg);
    SDL_UnlockMutex(q->mutex);

    return ret;
}

inline static void msg_init_msg(AVMessage *msg)
{
    memset(msg, 0, sizeof(AVMessage));
}

inline static void msg_queue_put_simple1(MessageQueue *q, int what)
{
    AVMessage msg;
    msg_init_msg(&msg);
    msg.what = what;
    msg_queue_put(q, msg);
}

inline static void msg_queue_put_simple2(MessageQueue *q, int what, int arg1)
{
    AVMessage msg;
    msg_init_msg(&msg);
    msg.what = what;
    msg.arg1 = arg1;
    msg_queue_put(q, &msg);
}

inline static void msg_queue_put_simple3(MessageQueue *q, int what, int arg1, int arg2)
{
    AVMessage msg;
    msg_init_msg(&msg);
    msg.what = what;
    msg.arg1 = arg1;
    msg.arg2 = arg2;
    msg_queue_put(q, &msg);
}

inline static void msg_queue_init(MessageQueue *q)
{
    memset(q, 0, sizeof(MessageQueue));
    q->mutex = SDL_CreateMutex();
    q->cond = SDL_CreateCond();
    q->abort_request = 1;
}

inline static void msg_queue_flush(MessageQueue *q)
{
    AVMessage *msg, *msg1;

    SDL_LockMutex(q->mutex);
    for (msg = q->first_msg; msg != NULL; msg = msg1) {
        msg1 = msg->next;
        av_freep(&msg);
    }
    q->last_msg = NULL;
    q->first_msg = NULL;
    q->nb_messages = 0;
    SDL_UnlockMutex(q->mutex);
}

inline static void msg_queue_destroy(MessageQueue *q)
{
    msg_queue_flush(q);
    SDL_DestroyMutex(q->mutex);
    SDL_DestroyCond(q->cond);
}

inline static void msg_queue_abort(MessageQueue *q)
{
    SDL_LockMutex(q->mutex);

    q->abort_request = 1;

    SDL_CondSignal(q->cond);

    SDL_UnlockMutex(q->mutex);
}

inline static void msg_queue_start(MessageQueue *q)
{
    SDL_LockMutex(q->mutex);
    q->abort_request = 0;
    msg_queue_put_simple1(q, FFP_MSG_FLUSH);
    SDL_UnlockMutex(q->mutex);
}

/* return < 0 if aborted, 0 if no msg and > 0 if msg.  */
static int msg_queue_get(MessageQueue *q, AVMessage *msg, int block)
{
    AVMessage *msg1;
    int ret;

    SDL_LockMutex(q->mutex);

    for (;;) {
        if (q->abort_request) {
            ret = -1;
            break;
        }

        msg1 = q->first_msg;
        if (msg1) {
            q->first_msg = msg1->next;
            if (!q->first_msg)
                q->last_msg = NULL;
            q->nb_messages--;
            *msg = *msg1;
            av_free(msg1);
            ret = 1;
            break;
        } else if (!block) {
            ret = 0;
            break;
        } else {
            SDL_CondWait(q->cond, q->mutex);
        }
    }
    SDL_UnlockMutex(q->mutex);
    return ret;
}

#endif