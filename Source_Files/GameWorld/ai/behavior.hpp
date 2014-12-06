

class monsterBehavior
{
public:
 monsterBehavior() {}
 inline int operator()(Monster *me)
 {
  return behave(me);
 }
 
private:
 virtual int behave(Monster *me) = 0;
 /*
  the name of the behavior
 */
 char* behaviorName;  
 
 /*
  the behavior's class
 */
 char* behaviorClass;
};
