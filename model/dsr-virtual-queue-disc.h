/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
#ifndef DSR_VIRTUAL_QUEUE_DISC_H
#define DSR_VIRTUAL_QUEUE_DISC_H

#include "ns3/queue-disc.h"

namespace ns3 {

class DsrVirtualQueueDisc : public QueueDisc {
public:
  /**
   * \brief Get the type ID.
   * \return the object TypeId
   */
  static TypeId GetTypeId (void);
  /**
   * \brief PfifoFastQueueDisc constructor
   *
   * Creates a queue with a depth of 1000 packets per band by default
   */
  DsrVirtualQueueDisc ();

  virtual ~DsrVirtualQueueDisc();

  // Reasons for dropping packets
  static constexpr const char* LIMIT_EXCEEDED_DROP = "Queue disc limit exceeded";  //!< Packet dropped due to queue disc limit exceeded
  static constexpr const char* TIMEOUT_DROP = "time out !!!!!!!!";
  static constexpr const char* BUFFERBLOAT_DROP = "Buffer bloat !!!!!!!!";

private:
  // packet size = 1kB
  // packet size for test = 52B
  uint32_t LinesSize[3] = {12,36,100}; // Buffer size
  uint32_t m_delayRef [3] = {1, 2, 100}; // Delay upper bound
  uint32_t m_fastWeight = 10; 
  uint32_t m_slowWeight = 5;
  uint32_t m_normalWeight = 0;
  uint32_t m_remainWeight = 0;
  uint32_t m_arrivals[3] = {0, 0, 0}; // arrival of Period
  // uint32_t m_farr = 0; // number of arriving packets at 1st queue
  // uint32_t m_sarr = 0; // number of arriving packets at 2nd queue
  // uint32_t m_narr = 0; // number of arriving packets at best-effort queue
  uint32_t m_queueLength[3] = {0, 0, 0}; // Real Queue length
  // uint32_t m_fql = 0; // 1st priority queue, queue length in packets
  // uint32_t m_sql = 0; // 2nd 
  // uint32_t m_nql = 0; // best effort queue
  double m_estDropOld[3] = {0.0, 0.0, 0.0}; // estimated drop probability n-1 th
  double m_estDropNew[3] = {0.0, 0.0, 0.0}; // estimated drop probability n th
  double m_optDrop[3] = {0.0, 0.0, 0.0}; // optimal drop probability
  uint32_t m_estQlOld[3] = {0, 0, 0}; // in packets
  uint32_t m_estQlNew[3] = {0, 0, 0}; // in packets
  uint32_t m_toDrop[3] = {0, 0, 0}; // in packets
  uint32_t m_usedTokens[3] = {0, 0, 0}; // tokens used
  double m_gamma[3] = {0.8, 0.4, 0.1};
  

  uint32_t currentFastWeight = 0;
  uint32_t currentSlowWeight = 0;
  uint32_t currentNormalWeight = 0;
  virtual bool DoEnqueue (Ptr<QueueDiscItem> item);
  virtual Ptr<QueueDiscItem> DoDequeue (void);
  // virtual void DoPrioDequeue (void);
  virtual Ptr<const QueueDiscItem> DoPeek (void);
  virtual bool CheckConfig (void);
  virtual void InitializeParams (void);
  virtual uint32_t Classify (); 
  virtual uint32_t EnqueueClassify (Ptr<QueueDiscItem> item);
  // virtual uint32_t QueueLengthUpdate (uint32_t m_queueLength, uint32_t m_estQlOld, uint32_t m_estQlNew);
  // virtual uint32_t DropProbUpdate (double m_estDropNew, uint32_t m_OptDrop, uint32_t m_estDropOld, uint32_t m_estQlNew, uint32_t m_TODrop, uint32_t m_arrivals);
  // virtual double DropProbEstimate (uint32_t m_estQlNew, uint32_t m_TODrop, uint32_t m_arrivals, uint32_t m_delayRef, uint32_t m_queueLength, uint32_t m_gamma);
  // virtual uint32_t QueueEstimate (uint32_t m_toDrop, uint32_t m_usedTokens, uint32_t m_arrivals, uint32_t m_queueLength);
  virtual uint32_t QueueLengthUpdate (void);
  virtual uint32_t DropProbUpdate (void);
  virtual double DropProbEstimate (void);
  virtual uint32_t QueueEstimate (void);
};

}

#endif /* DSR_VIRTUAL_QUEUE_DISC_H */
