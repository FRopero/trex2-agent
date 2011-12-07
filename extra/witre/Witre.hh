/*********************************************************************
 * Software License Agreement (BSD License)
 *
 *  Copyright (c) 2011, MBARI.
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above
 *     copyright notice, this list of conditions and the following
 *     disclaimer in the documentation and/or other materials provided
 *     with the distribution.
 *   * Neither the name of the TREX Project nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 *  FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 *  COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 *  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 *  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 *  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 *  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 *  ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 */
#ifndef H_Witre
# define H_Witre

#include <Wt/WApplication>
#include <Wt/WIntValidator>
#include <Wt/WWidget>
#include <Wt/WBreak>
#include <Wt/WContainerWidget>
#include <Wt/WLineEdit>
#include <Wt/WPushButton>
#include <Wt/WText>
#include <Wt/WTimer>
#include <Wt/WBreak>
#include <Wt/WTime>
#include <Wt/WPanel>
#include <Wt/WAnimation>
#include <Wt/WFlags>
#include <Wt/WGroupBox>
#include <Wt/WCheckBox>
#include <Wt/WComboBox>
#include <Wt/WDialog>
#include <Wt/WMessageBox>
#include <Wt/WDateTime>
#include <string>
#include <queue>
#include <map>

#include <boost/version.hpp>

#include "WitreServer.hh"
#include "Observations.hh"
#include "Popup.hh"

namespace TREX {
  namespace witre {

    class WitreApplication :public Wt::WApplication {

    private:
      Wt::WText *clock;
      Wt::WLineEdit *input;
      Wt::WPushButton *enter;
      Wt::WText *tickNum;
      Wt::WContainerWidget *messages;
      Wt::WTimer *timer;
      Wt::WComboBox *menu;
      Goalpopup* popup;
      WitreServer *wServer;

      std::map<std::string, bool> tLineMap;
      std::queue<Observations> observations;
      friend class WitreServer;

    public:
      WitreApplication(Wt::WEnvironment const &env, WitreServer* Server);
      ~WitreApplication();
      void post(); //Post the observations
      void addObs(std::string obs, std::string obj); //Adds observations to the queue
      void updateTick(std::string tick) { tickNum->setText(tick);}; //Updates the tickNum text
      int count(){ return messages->count();}; //Returns the number of messages
      Wt::WWidget * widget(int i) {return messages->widget(i);}; //Returns the widget at variable i in messages
      void insert(Wt::WPanel *wid); //Inserts widget at position 0
      void updateTime(); //Updates the time
      void timeLineChange(); //Updates when user changes what timelines to view
      void syncObservations(); //Syncs with server observations
      void attributePopup();
      void clientPostGoal(transaction::IntegerDomain start, transaction::IntegerDomain duration, transaction::IntegerDomain end);
      void test();


    };

    WitreApplication *createWitre(Wt::WEnvironment const &e, WitreServer* Server);
  } // TREX::witre
} // TREX

#endif // H_Witre
