Team Members:

1. Jiacheng Wu:    jcwu22@cs.washington.edu
2. Yuan-Mao Chueh: ymchueh@cs.washington.edu

Project Description:

Some Discussions:

    Suppose we have proposer P1, P2, P3 (If we use P means the alive distinguished leader)
    Suppoer we heve acceptor A1, A2, A3

    * Proposer P need to identify which PREPARE numbered n the PROMISE message of Acceptor replied to!!
        Image an extreme case:
        - P first send PREPARE to A1, A2, A3 with proposal number n = 3;
        - A1, A2, A3 recv PREPARE and PROMISE not to respond other proposal less than n = 3;
            - But these three message arrives late until the next propose action.
        - P find it is timeout to receive PROMISE, and repropose PREPARE with n = 6;
            - But these three message just lost and A1, A2, A3 didn't receive it.
        - P then receive message in the previous round with n = 3;
            - If we do not identify the proposal number,
              then P would think A1, A2, A3 promised not to respond other proposal with n = 6,
              even though now A1, A2, A3 never receive this PREPARE request with n = 6!!!
        - Then, it violates the Paxos algorithm, and may lead the acceptor to accept two different values!!
        Therefore, Propose P should identify which PREPARE numbered n the PROMISE message replied to.
        - If we received the out-of-dated PROMISE, that is the replied n < current proposal number
        - we should omit this PROMISE, and do nothing since new proposal is already start
        - That's also the reason for additional_field_1 in struct Message;

