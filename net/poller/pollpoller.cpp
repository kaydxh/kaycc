#include "pollpoller.h"

#include "../channel.h"
#include "../../base/types.h"

#include <errno.h>
#include <assert.h>
#include <poll.h>

using namespace kaycc;
using namespace kaycc::net;

PollPoller::PollPoller(EventLoop* loop)
    : Poller(loop) {

}

PollPoller::~PollPoller() {

}

Timestamp PollPoller::poll(int timeoutMs, ChannelList* activeChannels) {
    int numEvents = ::poll(&*pollfds_.begin(), pollfds_.size(), timeoutMs);
    int savedError = errno;

    Timestamp now(Timestamp::now());
    if (numEvents > 0) {
        LOG <<  __FILE__ << ":" << __LINE__ << ":" << __FUNCTION__ << " " << numEvents << " events happended." << std::endl;
        fillActiveChannels(numEvents, activeChannels);
    } else if (numEvents == 0) {
        LOG << "nothing happended." << std::endl;
    } else {
        if (savedError != EINTR) {
            errno = savedError; 
            LOG << "PollPoller::poll error not EINTER: " << savedError << std::endl;
        }
    }

    return now;
}

void PollPoller::fillActiveChannels(int numEvents, ChannelList* activeChannels) const {
    // 把发生了事件的事件处理器加入到已激活的事件处理器列表中  
    for (PollFdList::const_iterator pfd = pollfds_.begin();
        pfd != pollfds_.end(); ++pfd) {

        if (pfd->revents > 0) { //>=说明产生了事件 
            --numEvents; //处理一个减减
            ChannelMap::const_iterator ch = channels_.find(pfd->fd);
            assert(ch != channels_.end()); //一定找到

            Channel* channel = ch->second; //获取事件处理器
            assert(channel->fd() == pfd->fd);
            channel->set_revents(pfd->revents); //设置要返回的事件类型
            activeChannels->push_back(channel); //加入活跃事件数组
        }

    }

}

void PollPoller::updateChannel(Channel* channel) {
    Poller::assertInLoopThread();
    LOG << "fd=" << channel->fd() << " events=" << channel->events() << std::endl;;

    // a new one, add to pollfds_
    if (channel->index() < 0) {
        assert(channels_.find(channel->fd()) == channels_.end());

        struct pollfd pfd;
        pfd.fd = channel->fd();
        pfd.events = static_cast<short>(channel->events());
        pfd.revents = 0;
        pollfds_.push_back(pfd);
        int idx = static_cast<int>(pollfds_.size()) - 1; //
        channel->set_index(idx);
        channels_[pfd.fd] = channel;

    } else {
        assert(channels_.find(channel->fd()) != channels_.end());
        assert(channels_[channel->fd()] == channel);
        int idx = channel->index();
        assert(0 <= idx && idx < static_cast<int>(pollfds_.size()));

        struct pollfd& pfd = pollfds_[idx]; //获取pollfd，以引用方式提高效率
        assert(pfd.fd == channel->fd() || pfd.fd == -channel->fd()-1); //-channel->fd()-1是因为匹配下面暂时不关注状态的设置
        pfd.fd = channel->fd();
        pfd.events = static_cast<short>(channel->events());  //更新events 
        pfd.revents = 0;

        if (channel->isNoneEvent()) { 
            pfd.fd = -channel->fd() - 1; //这样子设置是为了removeChannel优化，减1，是考虑0的相反数仍为0
        }

    }
}

void PollPoller::removeChannel(Channel* channel) {
    Poller::assertInLoopThread();
    LOG << "fd=" << channel->fd() << std::endl;
    assert(channels_.find(channel->fd()) != channels_.end()); //删除必须能找到
    assert(channels_[channel->fd()] == channel);  //一定对应
    assert(channel->isNoneEvent()); //一定没有事件关注了
    int idx = channel->index();
    assert(0 <= idx && idx < static_cast<int>(pollfds_.size()));

    const struct pollfd& pfd = pollfds_[idx]; (void)pfd;
    assert(pfd.fd == -channel->fd() - 1 && pfd.events == channel->events());

    size_t n = channels_.erase(channel->fd()); //erase返回删除的数量
    assert(n == 1); (void)n;

    if (implicit_cast<size_t>(idx) == pollfds_.size() - 1) { //如果是最后一个, //直接pop_back() 
        pollfds_.pop_back();

    } else {
        int channelAtEnd = pollfds_.back().fd;
        iter_swap(pollfds_.begin()+idx, pollfds_.end() - 1);

        if (channelAtEnd < 0) {
            channelAtEnd = -channelAtEnd -1; //把它还原出来，得到真实的fd
        }

        channels_[channelAtEnd]->set_index(idx); //对该真实的fd更新下标 
        pollfds_.pop_back(); //弹出末尾元素
    }

}