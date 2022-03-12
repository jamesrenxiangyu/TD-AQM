/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
#ifndef DSR_VIRTUAL_QUEUE_DISC_H
#define DSR_VIRTUAL_QUEUE_DISC_H
#define N 3


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
  static constexpr const char* DROP_EARLY = "Early drop scheme";
  static constexpr const char* TIMEOUT_DROP = "time out !!!!!!!!";
  static constexpr const char* BUFFERBLOAT_DROP = "Buffer bloat !!!!!!!!";

private:
  // packet size = 1kB
  // packet size for test = 52B
  Ptr<UniformRandomVariable> m_rand;
  uint32_t LinesSize[3] = {12,36,100}; // Buffer size
  uint32_t m_delayRef [3] = {1, 2, 100}; // Delay upper bound
  // uint32_t m_fastWeight = 10;
  // uint32_t m_slowWeight = 5;
  // uint32_t m_normalWeight = 0;
  uint32_t m_tokens[N] = {10, 5, 0};
  uint32_t m_remainWeight = 0;
  uint32_t m_arrivals[3] = {0, 0, 0}; // arrival of Period
  // uint32_t m_farr = 0; // number of arriving packets at 1st queue
  // uint32_t m_sarr = 0; // number of arriving packets at 2nd queue
  // uint32_t m_narr = 0; // number of arriving packets at best-effort queue
  uint32_t m_qLNew[3] = {0, 0, 0}; // Real Queue length of time slot t
  uint32_t m_qLOld[3] = {0, 0, 0}; // Real Queue length of time slot t-1
  // uint32_t m_fql = 0; // 1st priority queue, queue length in packets
  // uint32_t m_sql = 0; // 2nd 
  // uint32_t m_nql = 0; // best effort queue
  // double m_tempDrop[3] = {0.0, 0.0, 0.0}; // temporal parameter for drop probability calculation
  double m_estDropOld[3] = {0.0, 0.0, 0.0}; // estimated drop probability n-1 th
  double m_estDropNew[3] = {0.0, 0.0, 0.0}; // estimated drop probability n th
  double m_optDrop[3] = {0.0, 0.0, 0.0}; // optimal drop probability
  uint32_t m_estQlOld[3] = {0, 0, 0}; // estimated queue length of time slot t (in packets)
  uint32_t m_estQlNew[3] = {0, 0, 0}; // estimated queue length of time slot t+1 (in packets)
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
  virtual void QueueLengthUpdate (void);
  virtual void DropProbUpdate (void);
  virtual void DropProbEstimate (Ptr<QueueDiscItem> item);
  virtual void QueueEstimate (void);
  virtual void ComputeDrop (double W[N], double b[N], bool opt);
  virtual std::vector<double> ComputeMatrix(double W_matrix[N][N], double b_vec[N]);

  virtual void getCofactor(double A_matrix[N][N], double temp[N], int p, int q, int n);
  virtual int determinant(double A_matrix[N][N], int n);
  virtual void adjoint(double A_matrix[N][N], double adj[N][N]);
  virtual std::array<std::array<double, N>, N>  inverse(double A_matrix[N][N], double inverse[N][N]);

};

}

#endif /* DSR_VIRTUAL_QUEUE_DISC_H */
