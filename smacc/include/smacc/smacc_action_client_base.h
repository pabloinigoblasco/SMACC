/*****************************************************************************************************************
 * ReelRobotix Inc. - Software License Agreement      Copyright (c) 2018
 * 	 Authors: Pablo Inigo Blasco, Brett Aldrich
 *
 ******************************************************************************************************************/
#pragma once

#include <smacc/smacc_action_client.h>
#include <queue>

namespace smacc
{
// Smacc Action Clients (AKA resources or plugins) can inherit from this object 
// inhteriting from this class works as a template .h library. That is why the code
// implementation is located here.
template <typename ActionType>
class SmaccActionClientBase: public ISmaccActionClient
{
    public:

    // inside this macro you can find the typedefs for Goal and other types
    ACTION_DEFINITION(ActionType);
    typedef actionlib::SimpleActionClient<ActionType> ActionClient ;
    typedef actionlib::SimpleActionClient<ActionType> GoalHandle;

    typedef typename ActionClient::SimpleDoneCallback SimpleDoneCallback ;
    typedef typename ActionClient::SimpleActiveCallback SimpleActiveCallback;
    typedef typename ActionClient::SimpleFeedbackCallback SimpleFeedbackCallback;

    SmaccActionClientBase(int feedback_queue_size=10)
        :ISmaccActionClient()
    {
        feedback_queue_size_= feedback_queue_size;
    }

    virtual void init(ros::NodeHandle& nh) override
    {
        ISmaccActionClient::init(nh);
        client_ = std::make_shared<ActionClient>(name_,false) ;
    }

    virtual ~SmaccActionClientBase()
    {
    }

    virtual void cancelGoal()
    {
        ROS_INFO("Cancelling goal of %s", this->getName().c_str());
        client_->cancelGoal();
    }

    virtual SimpleClientGoalState getState() override
    {
        return client_->getState();
    }

    virtual bool hasFeedback() override
    {
        return !feedback_queue_.empty();
    }

    void sendGoal(Goal& goal)
    {
        ROS_INFO_STREAM("Sending goal to actionserver located in " << this->name_ <<"\"");
        
        if(!client_->isServerConnected())
        {
            ROS_INFO("%s [at %s]: not connected with actionserver, waiting ..." , getName().c_str(), getNamespace().c_str());
            client_->waitForServer();
        }

        ROS_INFO_STREAM(getName()<< ": Goal Value: " << std::endl << goal);

        SimpleDoneCallback done_cb ;
        SimpleActiveCallback active_cb;
        SimpleFeedbackCallback feedback_cb = boost::bind(&SmaccActionClientBase<ActionType>::onFeedback,this,_1);

        client_->sendGoal(goal,done_cb,active_cb,feedback_cb);

        stateMachine_->registerActionClientRequest(this);
    }

protected:
    std::shared_ptr<ActionClient> client_;
    int feedback_queue_size_;
    std::list<Feedback> feedback_queue_;

    void onFeedback(const FeedbackConstPtr & feedback)
    {
        Feedback copy = *feedback;
        feedback_queue_.push_back(copy);
        ROS_DEBUG("FEEDBACK MESSAGE RECEIVED, Queue Size: %ld", feedback_queue_.size());
        if(feedback_queue_.size()> feedback_queue_size_)
        {
            feedback_queue_.pop_front();
        }
    }

    virtual void postEvent(SmaccScheduler* scheduler, SmaccScheduler::processor_handle processorHandle) override
    {
        EvActionResult<Result>* ev = new EvActionResult<Result>();

        boost::intrusive_ptr< EvActionResult<Result>> actionClientResultEvent = ev;
        actionClientResultEvent->client = this;

        scheduler->queue_event(processorHandle, actionClientResultEvent);
    }
    
    virtual void postFeedbackEvent(SmaccScheduler* scheduler, SmaccScheduler::processor_handle processorHandle) override
    {
        boost::intrusive_ptr< EvActionFeedback<Feedback> > actionFeedbackEvent = new EvActionFeedback<Feedback>();
        actionFeedbackEvent->client = this;

        bool ok = false;
        if(!feedback_queue_.empty())
        {
            ROS_DEBUG("[%s]Popping FEEDBACK MESSAGE, Queue Size: %ld", this->getName().c_str(), feedback_queue_.size());
            Feedback feedback_msg = feedback_queue_.front();
            feedback_queue_.pop_front();
            ROS_DEBUG("[%s]popped FEEDBACK MESSAGE, Queue Size: %ld", this->getName().c_str(), feedback_queue_.size());
            actionFeedbackEvent->feedbackMessage = feedback_msg;
            ok = true;
        }
        else
        {
            ok = false;
        };

        if(ok)
        {
            //ROS_INFO("Sending feedback event");
            scheduler->queue_event(processorHandle, actionFeedbackEvent);
        }
    }    

    friend class SignalDetector;
};
}