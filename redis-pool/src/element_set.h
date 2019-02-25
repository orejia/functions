#ifndef __FRAME_ELEMENT_SET__
#define __FRAME_ELEMENT_SET__

#include <stdint.h>
#include <limits.h>
#include <stdlib.h>
#include <stddef.h>

#include "util.h"

template <typename T>
class element_set_t
{
public:
    static const uint32_t END_OF_ELEMENT = UINT_MAX;

public:
    element_set_t() : m_elements(NULL) {}

    uint32_t first() const
    {
        return m_first_used;
    }

    uint32_t next(uint32_t pos) const
    {
        return m_elements[pos].next;
    }

    int init(uint32_t element_count, void (*init_proc)(T&, void*) = NULL, void *param = NULL)
    {
        if (m_elements)
        {
            return -1;
        }

        if (!element_count || UINT_MAX == element_count)
        {
            return -1;
        }

        m_elements = (element_t*)malloc(sizeof(element_t) * element_count);
        if (!m_elements)
        {
            return -1;
        }

        for (uint32_t i = 0; i < element_count - 1; ++ i)
        {
            m_elements[i].next = i + 1;
            m_elements[i].free = 1;
        }
        m_elements[element_count - 1].next = END_OF_ELEMENT;
        m_elements[element_count - 1].free = 1;

        if (init_proc)
        {
            for (uint32_t i = 0; i < element_count; ++ i)
            {
                init_proc(m_elements[i].data, param);
            }
        }

        m_first_free = 0;
        m_last_free = element_count - 1;
        m_first_used = END_OF_ELEMENT;
        m_element_count = element_count;

        return 0;
    }

    void uninit(void (*uninit_proc)(T&, void*) = NULL, void *param = NULL)
    {
        if (m_elements)
        {
            if (uninit_proc)
            {
                for (uint32_t i = m_first_used; i != END_OF_ELEMENT; i = m_elements[i].next)
                {
                    uninit_proc(m_elements[i].data, param);
                }
            }

            free(m_elements);
            m_elements = NULL;
        }
    }

    bool is_valid_subscript(uint32_t subscript)   
    {
        return m_elements && (subscript < m_element_count) && !m_elements[subscript].free;
    }

    int put_elem(uint32_t subscript)
    {
        if (unlikely(!m_elements || subscript >= m_element_count || m_elements[subscript].free))
        {
            return -1;
        }

        if (likely(m_elements[subscript].prev != END_OF_ELEMENT))
        {
            m_elements[m_elements[subscript].prev].next = m_elements[subscript].next;
        }
        if (likely(m_elements[subscript].next != END_OF_ELEMENT))
        {
            m_elements[m_elements[subscript].next].prev = m_elements[subscript].prev;
        }
        if (unlikely(m_first_used == subscript))
        {
            m_first_used = m_elements[subscript].next;
        }

        m_elements[subscript].next = END_OF_ELEMENT;
        if (likely(END_OF_ELEMENT != m_last_free))
        {
            m_elements[m_last_free].next = subscript;
        }
        m_last_free = subscript;
        if (unlikely(END_OF_ELEMENT == m_first_free))
        {
            m_first_free = subscript;
        }
        m_elements[subscript].free = 1;

        return 0;
    }

    uint32_t get_elem()
    {
        if (unlikely(!m_elements || END_OF_ELEMENT == m_first_free))
        {
            return END_OF_ELEMENT;
        }

        uint32_t temp = m_first_free;
        m_first_free = m_elements[m_first_free].next;
        if (unlikely(m_last_free == temp))
        {
            m_last_free = END_OF_ELEMENT;
        }

        m_elements[temp].free = 0;
        m_elements[temp].prev = END_OF_ELEMENT;
        m_elements[temp].next = m_first_used;

        if (likely(END_OF_ELEMENT != m_first_used))
        {
            m_elements[m_first_used].prev = temp;
        }
        m_first_used = temp;

        return temp;
    }

    const T& operator [] (uint32_t subscript) const
    {
        return const_cast<element_set_t*>(this)->operator [](subscript);
    }

    T& operator [] (uint32_t subscript)
    {
        return m_elements[subscript].data;
    }

private:
    typedef struct element_t
    {
        T data;
        uint32_t prev;
        uint32_t next;
        uint8_t free;
    } element_t;

    element_t *m_elements;
    uint32_t m_element_count;
    uint32_t m_first_free;
    uint32_t m_last_free;
    uint32_t m_first_used;
};

#endif
