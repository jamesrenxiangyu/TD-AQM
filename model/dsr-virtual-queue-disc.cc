/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#include "ns3/log.h"
#include "ns3/object-factory.h"
#include "ns3/queue.h"
#include "ns3/socket.h"
#include "ns3/packet.h"
#include "ns3/simulator.h"
#include "dsr-header.h"
#include "dsr-virtual-queue-disc.h"

#define FAST_LANE 0
#define SLOW_LANE 1
#define NORMAL_LANE 2
#define N 3

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("DsrVirtualQueueDisc");

NS_OBJECT_ENSURE_REGISTERED (DsrVirtualQueueDisc);

TypeId DsrVirtualQueueDisc::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::DsrVirtualQueueDisc")
    .SetParent<QueueDisc> ()
    .SetGroupName ("DsrRouting")
    .AddConstructor<DsrVirtualQueueDisc> ()
    .AddAttribute ("MaxSize",
                   "The maximum number of packets accepted by this queue disc.",
                   QueueSizeValue (QueueSize ("1000p")),
                   MakeQueueSizeAccessor (&QueueDisc::SetMaxSize,
                                          &QueueDisc::GetMaxSize),
                   MakeQueueSizeChecker ())
  ;
  return tid;
}

DsrVirtualQueueDisc::DsrVirtualQueueDisc ()
  : QueueDisc (QueueDiscSizePolicy::MULTIPLE_QUEUES, QueueSizeUnit::PACKETS)
{
  NS_LOG_FUNCTION (this);
}

DsrVirtualQueueDisc::~DsrVirtualQueueDisc ()
{
  NS_LOG_FUNCTION (this);
}


bool
DsrVirtualQueueDisc::DoEnqueue (Ptr<QueueDiscItem> item)
{
  NS_LOG_FUNCTION (this << item);
  uint32_t lane = EnqueueClassify (item);

  if (GetInternalQueue(lane)->GetCurrentSize ().GetValue() >= LinesSize[lane]) // Bufferbloat drop
  {
    DropBeforeEnqueue (item, LIMIT_EXCEEDED_DROP);
  }

  /**
   * \brief Compute Primal drop probability every WRR round, 
   * \todo generate rnd variable to decide whether drop or not
  */
  if (m_remainWeight == 0)
  {
    for (int i=0; i<N; i++)
    {
      m_qLOld[i] = m_qLNew[i];
      m_qLNew[i] = GetInternalQueue(i)->GetNPackets ();
    }
    QueueEstimate ();  // Compute m_estQlNew
    QueueLengthUpdate (); // Update m_estQlNew
    DropProbEstimate (item); // Compute m_estDropNew
    DropProbUpdate (); // Update m_estDropNew
    for (int i=0; i<N; i++) // Reset statistics
    {
      m_arrivals[i] = 0;
      m_toDrop[i] = 0;
      m_usedTokens[i] = 0;
    }
  }

  /**
   * \brief Compute Alternative drop probability for each packet
   * \return the real drop probability
  */
  DsrHeader header;
  double dropAlt;
  if (item->GetPacket ()->PeekHeader (header) == 0) // empty header, enqueue to BE lane
  {
    bool retval = GetInternalQueue (lane)->Enqueue (item);
    m_arrivals[lane] += 1;
    return retval;
  }
  
  uint32_t budget = header.GetBudget ();
  
  uint32_t qLength = GetInternalQueue (lane)->GetNPackets ();
  uint32_t numRound = (qLength + 1)/m_tokens[lane];
  uint32_t delayOpt = qLength + numRound * (15 - m_tokens[lane]);
  uint32_t delayWst = delayOpt + (15 - m_tokens[lane]);
  if (budget < delayOpt)
  {
    dropAlt = 1.0;
  }
  else if (budget > delayWst)
  {
    dropAlt = 0.0;
  }
  else
  {
    dropAlt = (budget - delayOpt)/(delayWst - delayOpt);
  }
  
  
  // Execute early drop by rnd
  int randInt = m_rand->GetInteger (1, 100);
  if (randInt < std::max(dropAlt, m_estDropNew[lane]) * 100) // select the maximum drop probability
  {
    DropBeforeEnqueue (item, DROP_EARLY);
  }

  bool retval = GetInternalQueue (lane)->Enqueue (item);
  m_arrivals[lane] += 1;
  return retval;
}

Ptr<QueueDiscItem>
DsrVirtualQueueDisc::DoDequeue (void)
{
  NS_LOG_FUNCTION (this);

  Ptr<QueueDiscItem> item;
  DsrHeader header;
  item->GetPacket ()->PeekHeader (header);
  
  uint32_t prio = Classify ();
  if (prio == 88)
  {
    return 0;
  }
  if (item = GetInternalQueue (prio)->Dequeue ())
    {
      NS_LOG_LOGIC ("Popped from band " << prio << ": " << item);
      NS_LOG_LOGIC ("Number packets band " << prio << ": " << GetInternalQueue (prio)->GetNPackets ());
      // std::cout << "++++++ Current Queue length: " << GetInternalQueue (prio)->GetNPackets () << " at band: " << item <<  std::endl;
      /**
       * \todo Count the number of time-out drops in each queue by compare the packet delay budget and its sojourn time
      */
      uint32_t budget = header.GetBudget ();
      Time timeRec = header.GetTxTime ();
      Time timeNow = Simulator::Now ().GetMicroSeconds ();
      uint32_t sojournTime = timeNow - timeRec;
      if (sojournTime > budget)
      {
        m_toDrop[prio] += 1;
        DropAfterDequeue(item, TIMEOUT_DROP);
      }
      return item;
    }
  NS_LOG_LOGIC ("Queue empty");
  return item;
}

Ptr<const QueueDiscItem>
DsrVirtualQueueDisc::DoPeek (void)
{
  NS_LOG_FUNCTION (this);

  Ptr<const QueueDiscItem> item;

  for (uint32_t i = 0; i < GetNInternalQueues (); i++)
    {
      if ((item = GetInternalQueue (i)->Peek ()) != 0)
        {
          NS_LOG_LOGIC ("Peeked from band " << i << ": " << item);
          NS_LOG_LOGIC ("Number packets band " << i << ": " << GetInternalQueue (i)->GetNPackets ());
          return item;
        }
    }

  NS_LOG_LOGIC ("Queue empty");
  return item;
}

bool
DsrVirtualQueueDisc::CheckConfig (void)
{
  // std::cout << "queue line Number = " << GetNInternalQueues () << std::endl;
  NS_LOG_FUNCTION (this);
  if (GetNQueueDiscClasses () > 0)
    {
      NS_LOG_ERROR ("DsrVirtualQueueDisc cannot have classes");
      return false;
    }

  if (GetNPacketFilters () != 0)
    {
      NS_LOG_ERROR ("DsrVirtualQueueDisc needs no packet filter");
      return false;
    }
  
  if (GetNInternalQueues () == 0)
    {
      // create 3 DropTail queues with GetLimit() packets each
      ObjectFactory factory;
      factory.SetTypeId ("ns3::DropTailQueue<QueueDiscItem>");
      factory.Set ("MaxSize", QueueSizeValue (GetMaxSize ()));
      AddInternalQueue (factory.Create<InternalQueue> ());
      AddInternalQueue (factory.Create<InternalQueue> ());
      AddInternalQueue (factory.Create<InternalQueue> ());
    }

  if (GetNInternalQueues () != 3)
    {
      NS_LOG_ERROR ("DsrVirtualQueueDisc needs 3 internal queues");
      return false;
    }

  if (GetInternalQueue (0)-> GetMaxSize ().GetUnit () != QueueSizeUnit::PACKETS ||
      GetInternalQueue (1)-> GetMaxSize ().GetUnit () != QueueSizeUnit::PACKETS ||
      GetInternalQueue (2)-> GetMaxSize ().GetUnit () != QueueSizeUnit::PACKETS)
    {
      NS_LOG_ERROR ("PfifoFastQueueDisc needs 3 internal queues operating in packet mode");
      return false;
    }

  for (uint8_t i = 0; i < 2; i++)
    {
      if (GetInternalQueue (i)->GetMaxSize () < GetMaxSize ())
        {
          NS_LOG_ERROR ("The capacity of some internal queue(s) is less than the queue disc capacity");
          return false;
        }
    }
  // std::cout << "The number of Internal Queues = "<< GetNInternalQueues () << std::endl;
  return true;
}

void
DsrVirtualQueueDisc::InitializeParams (void)
{
  NS_LOG_FUNCTION (this);
}


/**
 * \brief Realize WRR mechanism, i.e., Priority queues has guaranteed number of tokens, Best-effort queue use the left-over tokens
 * \return Which queue to serve next
*/

uint32_t
DsrVirtualQueueDisc::Classify ()
{
  if (currentFastWeight > 0)
    {
      if (!GetInternalQueue (0)->IsEmpty () && m_usedTokens[0] != m_tokens[0])
        {
          currentFastWeight--;
          m_usedTokens[0] += 1;
          return 0;
        }
      else
        {
          m_remainWeight += currentFastWeight;
          currentFastWeight = 0;
        }
    }
  if (currentSlowWeight > 0)
    {
      if (!GetInternalQueue (1)->IsEmpty () && m_usedTokens[1] != m_tokens[1])
        {
          currentSlowWeight--;
          m_usedTokens[1] += 1;
          return 1;
        }
      else
        {
          m_remainWeight += currentSlowWeight;
          currentSlowWeight = 0;
        }
    }
  if (m_remainWeight > 0)
    {
      if (!GetInternalQueue (2)->IsEmpty ())
        {
          m_remainWeight --;
          m_usedTokens[2] += 1;
          return 2;
        }
      else
        {
          m_remainWeight = 0;
          // currentNormalWeight = 0;
        }
    }
  currentFastWeight = m_tokens[0];
  currentSlowWeight = m_tokens[1];
  // currentNormalWeight = m_remainWeight;
  

  if (currentFastWeight > 0)
    {
      if (!GetInternalQueue (0)->IsEmpty () && m_usedTokens[0] != m_tokens[0] )
        {
          currentFastWeight--;
          m_usedTokens[0] += 1;
          return 0;
        }
      else
        {
          m_remainWeight += currentFastWeight;
          currentFastWeight = 0;
        }
    }
  if (currentSlowWeight > 0 )
    {
      if (!GetInternalQueue (1)->IsEmpty () && m_usedTokens[1] != m_tokens[1])
        {
          currentSlowWeight--;
          m_usedTokens[1] += 1;
          return 1;
        }
      else
        {
          m_remainWeight += currentSlowWeight;
          currentSlowWeight = 0;
        }
    }
 
  if (m_remainWeight > 0)
    {
      if (!GetInternalQueue (2)->IsEmpty ())
        {
          currentNormalWeight--;
          return 2;
        }
      else
        {
          currentNormalWeight = 0;
          m_remainWeight = 0;
        }
    }
  
  return 88;
}   

/**
 * \brief Enqueue packets to different queues depending on priority
 * \return Lane index
*/
uint32_t
DsrVirtualQueueDisc::EnqueueClassify (Ptr<QueueDiscItem> item)
{
  DsrHeader header;
  if (item->GetPacket()->PeekHeader (header) == 0) // Q: slow/loss of ACK may also lead to network congestion
  {
    std::cout << "Empty header" << std::endl;
    return NORMAL_LANE;
  }

  uint8_t priority = header.GetPriority ();

  switch (priority)
  {
  case 0x00:
    return FAST_LANE;
  case 0x01:
    return SLOW_LANE;
  default:
    return NORMAL_LANE;
  }
}


/**
 * \brief Update drop probability estimation
 * \param m_OptDrop Optimal drop probability of previous time period
 * \param m_estDropOld Estimated drop probability of the previous time period
 * \param m_estDropNew Estimated drop probability of the current time period
*/
void
// DsrVirtualQueueDisc::DropProbUpdate (double m_estDropNew, uint32_t m_OptDrop, uint32_t m_estDropOld, uint32_t m_delayRef, uint32_t m_queueLength, uint32_t m_gamma
//                                               uint32_t m_estQlNew, uint32_t m_TODrop, uint32_t m_arrivals)
DsrVirtualQueueDisc::DropProbUpdate (void)
{
  double eta = 0.5;
  for (int i=0; i<3; i++)
  {
    m_estDropNew[i] = m_estDropNew[i] + eta * (m_optDrop[i] - m_estDropOld[i]);
    m_estQlOld[i] = m_estQlNew[i];
  }
  
}



/**
 * \brief Find optimal drop probability for each queue by solving the optimization problem
 * \return Drop proability estimation
*/
void 
// DsrVirtualQueueDisc::DropProbEstimate (uint32_t m_estQlNew, uint32_t m_TODrop, uint32_t m_arrivals, uint32_t m_delayRef, uint32_t m_queueLength, uint32_t m_gamma)
DsrVirtualQueueDisc::DropProbEstimate (Ptr<QueueDiscItem> item)
{
  double alp = 0.5;
  double beta = 0.5;
  double linkRate = 5000.0; // Get link rate in Kbps
  uint32_t pktSize = item->GetPacket ()->GetSize () * 8; // Get packet size in bits
  double W[3] = {0,0,0}; // W matrix
  double b[3] = {0,0,0}; //b matrix
  double b_opt[3] = {0,0,0}; //b matrix

  for (int i=0; i<3; i++)
  {
    double a1 = 1 / (m_delayRef[i] * linkRate);
    uint32_t a2 = std::max(m_estQlNew[i] - m_toDrop[i] - m_usedTokens[i], (unsigned int) 0) * pktSize;
    uint32_t a2_opt = std::max(m_qLNew[i] - m_toDrop[i] - m_usedTokens[i], (unsigned int) 0) * pktSize;
    double a3 = m_arrivals[i];
    double b1 = m_toDrop[i]/a3;
    double A0 = 1 - a1 * a2;
    double A0_opt = 1 - a1 * a2_opt;
    double A1 = a1 * a3;
    double B0 = beta * b1;

    W[i] = -alp * A1 * beta * m_gamma[i];
    b[i] = (alp * A0 * beta + alp * A1 * B0) * m_gamma[i];
    b_opt[i] = (alp * A0_opt * beta + alp * A1 * B0) * m_gamma[i];
  }
  ComputeDrop (W[N], b[N], false);
  ComputeDrop (W[N], b_opt[N], true);
  
}


void
DsrVirtualQueueDisc::ComputeDrop (double W[N], double b[N], bool opt)
{
  double W_matrix[N][N] = {{0.0, 0.0, 0.0}, {0.0, 0.0, 0.0}, {0.0, 0.0, 0.0}};
  // Transform vector to diagonal matrix
  for (int i=0; i<3; i++)
  {
    for (int j=0; j<3; j++)
    {
      if (i == j)
      {
        W_matrix[i][j] = W[i];
      }
    }
  }
  std::vector<double> x_drop = ComputeMatrix (W_matrix, b); // Solve opt problem
  if (opt)
  {
    for (int i=0; i<3; i++)
    {
      m_optDrop[i] = 1.0 - x_drop[i];
      if (m_optDrop[i] < 0.0)
      {
        m_optDrop[i] = 0.0;
      }
      if (m_optDrop[i] > 1.0)
      {
        m_optDrop[i] = 1.0;
      }
    }
  }
  else
  {
    for (int i=0; i<3; i++)
    {
      m_estDropNew[i] = 1.0 - x_drop[i];
      if (m_estDropNew[i] < 0.0)
      {
        m_estDropNew[i] = 0.0;
      }
      if (m_estDropNew[i] > 1.0)
      {
        m_estDropNew[i] = 1.0;
      }
    }
  }

}




/**
 * \brief Estimate queue length of the next time period
 * \param m_toDrop number of time-out drops in the previous time period
 * \param m_usedTokens number of used tokens in the previous time period
 * \param m_arrivals number of arrived packets in the previous time period
 * \param m_qLNew queue length of current time period
*/
void
// DsrVirtualQueueDisc::QueueEstimate (uint32_t m_toDrop, uint32_t m_usedTokens, uint32_t m_arrivals, uint32_t m_queueLength)
DsrVirtualQueueDisc::QueueEstimate (void)
{
  uint32_t tempQl;
  for (int i=0; i<3; i++)
  {
    tempQl = std::max (m_qLNew[i] - m_usedTokens[i] - m_toDrop[i], (unsigned int) 0) + m_arrivals[i];
    m_estQlNew[i] = tempQl;
  }
}

/**
 * \brief Update queue length estimation by TD
 * \param m_queueLength Real queue length of previous time period
 * \param m_estQlOld Estimated queue length of the previous time period
 * \param m_estQlNew Estimated queue length of the current time period
 * \return Updated m_estQlNew
*/
void
// DsrVirtualQueueDisc::QueueLengthUpdate (uint32_t m_queueLength, uint32_t m_estQlOld, uint32_t m_estQlNew)
DsrVirtualQueueDisc::QueueLengthUpdate (void)
{
  double eta = 0.5; // adjusting param
  double tempQl;
  for (int i=0; i<3; i++)
  {
    tempQl = m_estQlNew[i] + eta * (m_qLOld[i] - m_estQlOld[i]);
    m_estQlNew[i] = (uint32_t) tempQl;
    m_estQlOld[i] = m_estQlNew[i];
  }
}

/**
 * \brief Compute the optimal drop probability
 * \return drop probabilty vector
*/
std::vector<double> 
DsrVirtualQueueDisc::ComputeMatrix(double W[N][N], double b[N])
{
  std::array<std::array<double, N>, N>  inv_W;
  std::vector<double> temp_drop = {0.0, 0.0, 0.0};
  inv_W = inverse(W[N][N]);
  double badinv[N][N] = {0.0};
  double temp;


  for (int i=0; i<N; i++)  // compute inv_w * b
  {
    for (int j=0; j<N; j++)
    {
      temp += inv_W[i][j] * b[j];
    }
    temp_drop[i] = -0.5 * temp;
  }
  return temp_drop;

}


void 
DsrVirtualQueueDisc::getCofactor(double A[N][N], double temp[N][N], int p, int q, int n)
{
    int i = 0, j = 0;
 
    // Looping for each element of the matrix
    for (int row = 0; row < n; row++)
    {
        for (int col = 0; col < n; col++)
        {
            //  Copying into temporary matrix only those element
            //  which are not in given row and column
            if (row != p && col != q)
            {
                temp[i][j++] = A[row][col];
 
                // Row is filled, so increase row index and
                // reset col index
                if (j == n - 1)
                {
                    j = 0;
                    i++;
                }
            }
        }
    }
}
 
/* Recursive function for finding determinant of matrix.
   n is current dimension of A[][]. */
int 
DsrVirtualQueueDisc::determinant(double A[N][N], int n)
{
    double D = 0; // Initialize result
 
    //  Base case : if matrix contains single element
    if (n == 1)
        return A[0][0];
 
    int temp[N][N]; // To store cofactors
 
    int sign = 1;  // To store sign multiplier
 
     // Iterate for each element of first row
    for (int f = 0; f < n; f++)
    {
        // Getting Cofactor of A[0][f]
        getCofactor(A, temp, 0, f, n);
        D += sign * A[0][f] * determinant(temp, n - 1);
 
        // terms are to be added with alternate sign
        sign = -sign;
    }
 
    return D;
}
 
// Function to get adjoint of A[N][N] in adj[N][N].
void 
DsrVirtualQueueDisc::adjoint(double A[N][N], double adj[N][N])
{
    if (N == 1)
    {
        adj[0][0] = 1;
        return;
    }
 
    // temp is used to store cofactors of A[][]
    int sign = 1; 
    double temp[N][N];
 
    for (int i=0; i<N; i++)
    {
        for (int j=0; j<N; j++)
        {
            // Get cofactor of A[i][j]
            getCofactor(A, temp, i, j, N);
 
            // sign of adj[j][i] positive if sum of row
            // and column indexes is even.
            sign = ((i+j)%2==0)? 1: -1;
 
            // Interchanging rows and columns to get the
            // transpose of the cofactor matrix
            adj[j][i] = (sign)*(determinant(temp, N-1));
        }
    }
}
 
// Function to calculate and store inverse, returns false if
// matrix is singular
std::array<std::array<double, N>, N> 
DsrVirtualQueueDisc::inverse(double A[N][N])
{
    // Find determinant of A[][]
    int det = determinant(A, N);
    std::array<std::array<double, N>, N>  inv = {0.0};

    if (det == 0)
    {
        std::cout << "Singular matrix, can't find its inverse";
        return inv;
    }
 
    // Find adjoint
    double adj[N][N];
    adjoint(A, adj);
 
    // Find Inverse using formula "inverse(A) = adj(A)/det(A)"
    for (int i=0; i<N; i++)
        for (int j=0; j<N; j++)
            inverse[i][j] = adj[i][j]/double(det);
 
    return inv;
}


} // namespace ns3