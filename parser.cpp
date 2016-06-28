#include "parser.h"

void writeWithIndention(std::stringstream& sstream, const string& indent, int count_indent, const string& s);

/* === ParserTreeItem === */
ParserTreeItem::ParserTreeItem(KeyType k, int row_position, int column_position)
    : row(row_position), column(column_position), key(k)
{
}

void ParserTreeItem::addText(const string& text_part)
{
    location_sequence_of_data.push_back(TEXT);
    texts.push_back(text_part);
}

void ParserTreeItem::addChild(ParserTreeItem& item)
{
    location_sequence_of_data.push_back(CHILD);
    childs.push_back(&item);
}

//==========================================================

/* === ParserTree === */
ParserTree::ParserTree(const string& text) : rude_text(text), root_item(new ParserTreeItem(KeyType(""), 0, 0))
{
}

bool ParserTree::createTree()
{
    /* Check text fo empty */
    if (rude_text.empty())
        return false;

    /* Find all keys postition and put result to key_positions
     *   with check for text correctness, sort if needed */
    bool saccess;
    saccess = findAllKeyPosition(rude_text);
    if (saccess == false)
        return false;
    if (!std::is_sorted(key_positions.cbegin(), key_positions.cend(),
                       [](KeyPositionType a, KeyPositionType b) { return a.begin_key_area_pos < b.begin_key_area_pos; }))
        std::sort(key_positions.begin(), key_positions.end(),
                  [](KeyPositionType a, KeyPositionType b) { return a.begin_key_area_pos < b.begin_key_area_pos; });

    /* Create tree by recursive function SubTree */
    try
    {
        unsigned int vector_position = 0;
        SubTree(0, rude_text.size(), vector_position, *root_item);
    }
    catch (std::bad_alloc)
    {
        return false;
    }

    return true;
}

void ParserTree::SubTree(unsigned int begin_rude_text_pos, unsigned int end_rude_text_pos,
                           unsigned int& vector_pos, ParserTreeItem& item)
{
    unsigned int text_pos = begin_rude_text_pos;        // position in rude_text
    unsigned int current_vector_pos = vector_pos;       // vector_pos = (max - 1) used key position in key_positions
                                                        // current_vector_pos = key position of this calling
    while (text_pos != end_rude_text_pos)
    {
        if (vector_pos < key_positions.size() && text_pos == key_positions[vector_pos].begin_key_area_pos)
        {
            ParserTreeItem * p_child = new ParserTreeItem(KeyType(rude_text.substr(text_pos, key_positions[vector_pos].begin_data_pos - text_pos)),
                                                                             item.row + 1, item.childs.size());
            item.addChild(*p_child);
            vector_pos++;
            SubTree(key_positions[vector_pos - 1].begin_data_pos, key_positions[vector_pos - 1].end_data_pos, vector_pos, *p_child);
            text_pos = key_positions[current_vector_pos + item.childs.size() - 1].end_key_area_pos;
        }
        else  // text_pos look on simply text (not key)
        {
            unsigned int end_temp_pos;
            if (vector_pos < key_positions.size() && key_positions[vector_pos].begin_key_area_pos < end_rude_text_pos)
                end_temp_pos = key_positions[vector_pos].begin_key_area_pos;
            else
                end_temp_pos = end_rude_text_pos;
            item.addText(rude_text.substr(text_pos, end_temp_pos - text_pos));
            text_pos = end_temp_pos;
        }
    }
}

ParserTree::~ParserTree()
{
    ParserTreeItem* pItem = root_item;
    ParserTreeItemStack stack;
    while(!pItem->childs.empty())
    {
        stack.push(pItem);
        pItem = pItem->childs[pItem->childs.size() - 1];
    }
    stack.push(pItem);

    while(stack.size() != 1)    // on top of stack always most column-right
    {                           // != 1 else pItem->child reference to hollowness and .pop_back() lead to error
        delete stack.top();
        stack.pop();
        pItem = stack.top();
        pItem->childs.pop_back();

        while (!pItem->childs.empty())
        {
            pItem = pItem->childs[pItem->childs.size() - 1];
            stack.push(pItem);
        }
    }
    delete stack.top();
}

// TODO: ParserTree::find()
ParserTree& ParserTree::find(const KeyType& key)
{
    if (key.name != "")
        return *this;
    return *this;
}

// NOTE: May be optimized
bool ParserTree::findAllKeyPosition(const string& s)
{
    KeyType keys[] = {
        KeyType("<html> </html>"), KeyType("<head> </head>"), KeyType("<body> </body>"),
        KeyType("<p> </p>")
    };
    unsigned int begin_key_area_pos, end_key_area_pos;  // [...)
    unsigned int begin_data_pos, end_data_pos;          // [...)
    unsigned int find_current_pos, find_end_pos;

    find_current_pos = 0;
    find_end_pos = s.size();
    for (KeyType current_key : keys)
    {
        while (find_current_pos != find_end_pos)
        {
            begin_key_area_pos = findBeginKeyAreaPosition(s, find_current_pos, current_key);
            if (begin_key_area_pos == string::npos)
                break;
            begin_data_pos = findBeginDataPosition(s, begin_key_area_pos, current_key);
            if (begin_data_pos == string::npos)
                return false;
            end_data_pos = findEndDataPosition(s, begin_data_pos, current_key);
            if (end_data_pos == string::npos)
                return false;
            end_key_area_pos = findEndKeyAreaPosition(s, end_data_pos, current_key);
            if (end_key_area_pos == string::npos)
                return false;

            key_positions.push_back(KeyPositionType(current_key, begin_key_area_pos, end_key_area_pos,
                                                   begin_data_pos, end_data_pos));
            find_current_pos = begin_data_pos;
        }
        find_current_pos = 0;
    }
    return true;
}

unsigned int ParserTree::findBeginKeyAreaPosition(const string& s, unsigned int begin_pos, const KeyType& key) const
{
    unsigned int whitespace_pos = key.name.find(' ');
    return s.find(key.name.substr(0, whitespace_pos - 1), begin_pos);
}

unsigned int ParserTree::findBeginDataPosition(const string& s, unsigned int begin_key_area_pos, const KeyType& key) const
{
    return s.find('>', begin_key_area_pos) + 1;
}

// TODO: Consider nested same-name keys
unsigned int ParserTree::findEndDataPosition(const string& s, unsigned int begin_data_pos, const KeyType& key) const
{
    int count_nested_same_name_keys = 0;
    unsigned int whitespace_pos = key.name.find(' ');
    unsigned int find_begin, find_end;
    unsigned int end_key_word;

    find_begin = findBeginKeyAreaPosition(s, begin_data_pos, key);
    find_end = s.find(key.name.substr(whitespace_pos + 1), begin_data_pos);

    while (find_begin < find_end)
    {
        count_nested_same_name_keys++;
        end_key_word = findBeginDataPosition(s, find_begin, key);
        find_begin = findBeginKeyAreaPosition(s, end_key_word, key);
    }
    while (count_nested_same_name_keys --> 0)
    {
        end_key_word = findEndKeyAreaPosition(s, find_end, key);
        find_end = s.find(key.name.substr(whitespace_pos + 1), end_key_word);
    }

    return find_end;
}

unsigned int ParserTree::findEndKeyAreaPosition(const string& s, unsigned int end_data_pos, const KeyType& key) const
{
    return s.find('>', end_data_pos) + 1;
}

void writeWithIndention(std::stringstream& sstream, const string& indent, int count_indent, const string& s)
{
    unsigned int last_pos = -1;
    unsigned int prev_pos;
    while (last_pos + 1 < s.size())      // write line by line
    {
        prev_pos = last_pos + 1;         // eat '\n'
        last_pos = s.find('\n', prev_pos);
        if (last_pos == string::npos)
            last_pos = s.size();

        for (int i = 0; i < count_indent; i++)
            sstream << indent;
        for (unsigned int pos = prev_pos; pos <= last_pos; pos++)
            sstream.put(s[pos]);
    }
}

string ParserTree::outResult() const
{
    struct ItemOutputState
    {
        unsigned int location_sequence_num;
        unsigned int text_num;
        unsigned int child_num;
    };
    ItemOutputState zero_state = {};

    typedef std::pair<ParserTreeItem*, ItemOutputState> ItemAndState;
    std::stack<ItemAndState, vector<ItemAndState>> stack;

    const ParserTreeItem* pItem;
    ItemOutputState* pState;
    std::stringstream sstream;

    stack.push(ItemAndState(root_item, zero_state));
    pItem = stack.top().first;
    pState = &stack.top().second;

    while (!stack.empty())
    {
        /* New key area */
        if (pState->location_sequence_num == 0)
        {
            writeWithIndention(sstream, "| ", pItem->row, string("@" + pItem->key.name + "\n"));
            writeWithIndention(sstream, "| ", pItem->row, "//====================\n");
        }

        /* Key data */
        if (pItem->location_sequence_of_data[pState->location_sequence_num] == ParserTreeItem::TEXT)
        {
            writeWithIndention(sstream, "| ", pItem->row + 1, "[" + pItem->texts[pState->text_num]);
            if (pItem->texts[pState->text_num].back() == '\n')
                writeWithIndention(sstream, "| ", pItem->row + 1, "]\n");
            else
                sstream << "]\n";
            pState->text_num++;
            pState->location_sequence_num++;
        }
        else
        {
            pState->child_num++;
            pState->location_sequence_num++;
            stack.push(ItemAndState(pItem->childs[pState->child_num - 1], zero_state));
        }

        /* Set last item-state */
        pItem = stack.top().first;
        pState = &stack.top().second;

        /* End key area */
        if (pState->location_sequence_num == pItem->location_sequence_of_data.size())
        {
            writeWithIndention(sstream, "| ", pItem->row, "\\\\====================\n");
            stack.pop();
            if (stack.empty() == false)
            {
                pItem = stack.top().first;
                pState = &stack.top().second;
            }
        }
    }

    return sstream.str();
}
