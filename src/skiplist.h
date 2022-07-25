
#include <vector>
#include <iostream>

// 跳跃表节点类型
template <class K, class V>
struct Node
{
public:
    Node() : key(K()), value(V()) {}
    Node(int);
    Node(const K &k, const V &v, int);
    ~Node() {}
    K getKey() const;
    V getValue() const;
    void setValue(const V &);
    int level;
    std::vector<Node<K, V> *> next; // 保存不同层指向下一个节点的指针
private:
    K key;
    V value;
};

template <class K, class V>
Node<K, V>::Node(int level_)
    : key(K()), value(V()), level(level_), next(level_ + 1, nullptr)
{
}

template <class K, class V>
Node<K, V>::Node(const K &k, const V &v, int level_)
    : key(k), value(v), level(level_), next(level_ + 1, nullptr)
{
}

template <class K, class V>
K Node<K, V>::getKey() const
{
    return key;
}

template <class K, class V>
V Node<K, V>::getValue() const
{
    return value;
}

template <class K, class V>
void Node<K, V>::setValue(const V &v)
{
    this->value = v;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// 跳跃表的迭代器，是一个只能向前的迭代器类型
template <class K, class V>
struct __skiplist_iterator
{
    typedef __skiplist_iterator<K, V> self;
    typedef Node<K, V> value_type;
    typedef Node<K, V> *link_type;
    typedef Node<K, V> &reference;
    typedef Node<K, V> *pointer;

    link_type node;

    __skiplist_iterator(link_type x) : node(x) {}
    __skiplist_iterator() {}

    bool operator==(const self &x) const
    {
        return node == x.node;
    }
    bool operator!=(const self &x) const
    {
        return node != x.node;
    }
    self &operator++()
    {
        node = node->next[0];
        return *this;
    }
    self operator++(int)
    {
        self tmp = *this;
        ++*this;
        return tmp;
    }
    reference operator*() const
    {
        return *node;
    }
    pointer operator->() const
    {
        return node;
    }
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

const int MAX_LEVEL = 8;

// 跳跃表
template <class K, class V>
class SkipList
{
public:
    typedef __skiplist_iterator<K, V> iterator;

    SkipList();
    SkipList(int);
    ~SkipList();
    int getRandomLevel();
    Node<K, V> *createNode(const K &, const V &, int);
    bool insert(const K &, const V &);
    bool find(const K &, V &);
    bool erase(const K &);
    bool modify(const K &, const V &);
    unsigned int size() const;
    Node<K, V> *getHeader() const { return this->header; }
    iterator begin() const { return iterator(this->header->next[0]); }
    iterator end() const { return nullptr; }

private:
    int max_level;
    int cur_level;
    Node<K, V> *header;
    unsigned int element_count;
};

template <class K, class V>
SkipList<K, V>::SkipList()
    : max_level(MAX_LEVEL), cur_level(0), element_count(0),
      header(new Node<K, V>(MAX_LEVEL))
{
}

template <class K, class V>
SkipList<K, V>::SkipList(int max_level_)
    : max_level(max_level_), cur_level(0), element_count(0),
      header(new Node<K, V>(max_level_))
{
}

template <typename K, typename V>
SkipList<K, V>::~SkipList()
{
    delete header;
}

template <class K, class V>
int SkipList<K, V>::getRandomLevel()
{
    int k = 1;
    while (rand() % 2)
    {
        k++;
    }
    k = (k < max_level) ? k : max_level;
    return k;
}

template <class K, class V>
bool SkipList<K, V>::insert(const K &key, const V &value)
{
    Node<K, V> *cur_node = this->header;
    // 更新数组，保存每一层插入位置的前一个节点
    std::vector<Node<K, V> *> update(max_level + 1, nullptr);
    // 从跳跃表的最高层进行查找
    for (int i = cur_level; i >= 0; i--)
    {
        while (cur_node->next[i] != nullptr && cur_node->next[i]->getKey() < key)
        {
            cur_node = cur_node->next[i];
        }
        update[i] = cur_node;
    }
    cur_node = cur_node->next[0]; // 最底层上插入位置的节点
    if (cur_node != nullptr && cur_node->getKey() == key)
    { // 当前 key 已存在于跳跃表中
        return false;
    }
    if (cur_node == nullptr || cur_node->getKey() != key)
    {                                        // 进行插入操作
        int random_level = getRandomLevel(); // 生成一个随机层数
        if (random_level > cur_level)
        { // 如果生成的随机层数大于当前的层数，
            for (int i = cur_level + 1; i <= random_level; i++)
            {
                update[i] = header; // 那么当前层数以上的层，其插入位置的前一个节点都置为头节点
            }
            cur_level = random_level; // 更新当前层数
        }
        Node<K, V> *node = createNode(key, value, random_level); // 根据随机层数生成一个新的节点
        // 新节点的插入和普通单链表类似
        for (int i = 0; i <= random_level; i++)
        {
            node->next[i] = update[i]->next[i];
            update[i]->next[i] = node;
        }
        element_count++;
    }
    return true;
}

template <class K, class V>
bool SkipList<K, V>::modify(const K &key, const V &value)
{
    Node<K, V> *cur_node = this->header;
    std::vector<Node<K, V> *> update(max_level + 1, nullptr);
    for (int i = cur_level; i >= 0; i--)
    {
        while (cur_node->next[i] != nullptr && cur_node->next[i]->getKey() < key)
        {
            cur_node = cur_node->next[i];
        }
        update[i] = cur_node;
    }
    cur_node = cur_node->next[0];
    if (cur_node == nullptr || cur_node->getKey() != key)
    {
        return false;
    }
    if (cur_node != nullptr && cur_node->getKey() == key)
    {
        cur_node->setValue(value);
    }
    return true;
}

// 查找实际上是插入的简化版本
template <class K, class V>
bool SkipList<K, V>::find(const K &key, V &value)
{
    Node<K, V> *cur_node = this->header;
    // 从跳跃表的最高层进行查找
    for (int i = cur_level; i >= 0; i--)
    {
        while (cur_node->next[i] != nullptr && cur_node->next[i]->getKey() < key)
        {
            cur_node = cur_node->next[i];
        }
    }
    cur_node = cur_node->next[0];
    if (cur_node != nullptr && cur_node->getKey() == key)
    { // 当前 key 已存在于跳跃表中
        value = cur_node->getValue();
        return true;
    }
    return false;
}

template <class K, class V>
bool SkipList<K, V>::erase(const K &key)
{
    Node<K, V> *cur_node = this->header;
    std::vector<Node<K, V> *> update(max_level + 1, nullptr);
    for (int i = cur_level; i >= 0; i--)
    {
        while (cur_node->next[i] != nullptr && cur_node->next[i]->getKey() < key)
        {
            cur_node = cur_node->next[i];
        }
        update[i] = cur_node;
    }
    cur_node = cur_node->next[0];
    if (cur_node != nullptr && cur_node->getKey() == key)
    { // 找到需要删除的节点
        // 从最底层向上逐层删除节点
        for (int i = 0; i < cur_level; i++)
        {
            if (update[i]->next[i] != cur_node)
            { // 如果当前层的下一个节点不是要删除的目标节点，则跳出循环
                break;
            }
            update[i]->next[i] = cur_node->next[i];
        }
        // 去除没有元素的层
        while (cur_level > 0 && header->next[cur_level] == nullptr)
        {
            cur_level--;
        }
        element_count--;
        return true;
    }
    return false;
}

template <class K, class V>
unsigned int SkipList<K, V>::size() const
{
    return element_count;
}

template <class K, class V>
Node<K, V> *SkipList<K, V>::createNode(const K &k, const V &v, int level)
{
    return new Node<K, V>(k, v, level);
}
