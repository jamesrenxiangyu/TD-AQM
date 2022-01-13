/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2022 Xiangyu Ren
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Authors: Xiangyu Ren <jamesrxy@uvic.ca>
 *          
 *          
 */

/*
 * PORT NOTE: This code was ported from ns-2.36rc1 (queue/pie.h).
 * Most of the comments are also ported from the same.
 */

#ifndef TD_QUEUE_DISC_H
#define TD_QUEUE_DISC_H

#include "ns3/queue-disc.h"
#include "ns3/nstime.h"
#include "ns3/boolean.h"
#include "ns3/data-rate.h"
#include "ns3/timer.h"
#include "ns3/event-id.h"
#include "ns3/random-variable-stream.h"

#define BURST_RESET_TIMEOUT 1.5

class TdQueueDiscTestCase;  // Declaration
namespace ns3 {

class TraceContainer;
class UniformRandomVariable;

/**
 * \ingroup traffic-control
 * 
 * \brief Implements TD Active Queue Management discipline
 * 
*/
class TdQueueDisc : public QueueDisc
{
  public:
    /**
     * \brief Get the type ID.
     * \return the object TypeID
    */
    static TypeId GetTypeId (void);

    /**
     * \brief TdQueueDisc Constructor
    */
    TdQueueDisc ();

    /**
     * \brief TdQueueDisc Destructor
    */
    ~TdQueueDisc ();

    /**
     * \brief Burst types
    */
    enum BurstStateT
    {
      NO_BURST,
      IN_BURST,
      IN_BURST_PROTECTING,
    };

    /**
     * \brief Get queue delay.
     * 
     * \return The current queue delay.
    */
    Time GetQueueDelay (void);

    /**
     * Assign a fixed random variable stream number to the random variables
     * used by the model. 
     * 
     * \param stream first stream index to use
     * \return the number of stream indices assigned by this model
    */
    int64_t AssignStreams (int64_t stream);

    // Reasons for dropping packets
    static constexpr const char* ENQUEUE_DROP = "Enqueue drop"; // Drop before enqueue: proactive
    static constexpr const char* ENQUEUE_MARK = "Enqueue mark"; // Mark ECN bit before enqueue: proactive
    static constexpr const char* TIMEOUT_DROP = "Timeout drop"; // Drop due to timeout: proactive
    static constexpr const char* FORCED_DROP = "Forced drop"; // Drop due to queue limit: reactive

  protected:
    /**
     * \brief Dispose of the object
    */
    virtual void DoDispose (void);

  private:
    friend class::TdQueueDiscTestCase; // Test
    virtual bool DoEnqueue (Ptr<QueueDiscItem> item);
    virtual Ptr<QueueDiscItem> DoDequeue (void);
    virtual bool CheckConfig (void);

    /**
   * \brief Initialize the queue parameters.
   */
    virtual void InitializeParams (void);

    /**
     * \brief Check if a packet needs to be drop early according to drop probabiltiy
     * \param item queue item
     * \param item queue size
     * \return 0 for enqueue, 1 for drop
    */
    bool DropEarly (Ptr<QueueDiscItem> item, uint32_t qSize);

    /**
     * Calculate drop probability every WRR schedule round based on the state observations;
     * The primal drop probability is adjusted according to previous calculation results
    */
    void CalculateP ();

    // /**
    //  * \brief Check if a packet needs to be dropped due to exceeding time budget
    //  * \param item queue item
    //  * \return 0 for dequeue, 1 for drop
    // */
    // bool TimeOutDrop (Ptr<QueueDiscItem> item);

    // static const uint64_t DQCOUNT_INVALID = std::numeric_limits<uint64_t>::max(); // Invalid DqCount value



  // ** Variables supplied by user
  Time m_sUpdate;                               //!< Start time of the update timer
  uint64_t m_tUpdate;                           //!< Number of time slots of each WRR round
  double m_qDelayRef;                           //!< Desired queue delay: Buffer size dicount factor
  uint32_t m_meanPktSize;                       //!< Average packet size in bytes
  Time m_maxBurst;                              //!< Maximum burst allowed before random early dropping kicks in
  double m_a;                                   //!< Parameter to TD controller
  double m_b;                                   //!< Parameter to TD controller
  double m_n1;                                  //!< Parameter to adjuest Queue length estimation
  double m_n2;                                  //!< Parameter to adjuest Drop probability
  // uint32_t m_dqThreshold;                       //!< Minimum queue size in bytes before dequeue rate is measured
  // bool m_useDqRateEstimator;                    //!< Enable/Disable usage of dequeue rate estimator for queue delay calculation
  // bool  m_isCapDropAdjustment;                  //!< Enable/Disable Cap Drop Adjustment feature mentioned in RFC 8033
  bool m_useEcn;                                //!< Enable ECN Marking functionality
  // bool m_useDerandomization;                    //!< Enable Derandomization feature mentioned in RFC 8033
  double m_markEcnTh;                           //!< ECN marking threshold (default 10% as suggested in RFC 8033)
  // Time m_activeThreshold;                       //!< Threshold for activating PIE (disabled by default)

  // ** Variables maintained by TD-AQM
  uint32_t m_todrop;                            //!< number of drops due to time out.
  uint32_t m_arrival;                            //!< number of arrived packets for each queue.
  uint32_t m_tokens;                            //!< number of tokens used in each queue per round.
  double m_dropProb;                            //!< Variable used in calculation of drop probability
  Time m_qDelayOld;                             //!< Old estimation of queue delay
  Time m_qDelay;                                //!< True value of queue delay
  // Time m_burstAllowance;                        //!< Current max burst value in seconds that is allowed before random drops kick in
  // uint32_t m_burstReset;                        //!< Used to reset value of burst allowance
  // BurstStateT m_burstState;                     //!< Used to determine the current state of burst
  // bool m_inMeasurement;                         //!< Indicates whether we are in a measurement cycle
  // double m_avgDqRate;                           //!< Time averaged dequeue rate
  // double m_dqStart;                             //!< Start timestamp of current measurement cycle
  // uint64_t m_dqCount;                           //!< Number of bytes departed since current measurement cycle starts
  EventId m_rtrsEvent;                          //!< Event used to decide the decision of interval of drop probability calculation
  Ptr<UniformRandomVariable> m_uv;              //!< Rng stream
  // double m_accuProb;                            //!< Accumulated drop probability
  bool m_active;                                //!< Indicates whether PIE is in active state or not
};

};   // namespace ns3

#endif

