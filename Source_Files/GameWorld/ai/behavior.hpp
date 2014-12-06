

class monsterBehavior
{
public:
 monsterBehavior() {}
 inline int operator()(Monster *me)
 {
  return behave(me);
 }
 
 inline const char* getName()
 {
  return behaviorName;
 }
 inline const char* getClass()
 {
  return behaviorClass;
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
