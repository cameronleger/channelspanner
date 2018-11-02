#ifndef CHANNELSPANNER_LOGGING_H
#define CHANNELSPANNER_LOGGING_H

#ifndef __FILENAME__
#define __FILENAME__ (__FILE__ + SOURCE_PATH_SIZE)
#endif

#ifdef NDEBUG
#define DEBUG_PRINT( fmt, args... ) /* Don't do anything in release builds */
#else
#define DEBUG_PRINT( fmt, args... ) fprintf(stderr, "DEBUG: %s:%d:%s(): " fmt, __FILENAME__, __LINE__, __func__, ##args)
#endif

#endif //CHANNELSPANNER_LOGGING_H
