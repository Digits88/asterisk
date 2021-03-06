/*
 * Asterisk -- An open source telephony toolkit.
 *
 * Copyright (C) 2012 - 2013, Digium, Inc.
 *
 * Joshua Colp <jcolp@digium.com>
 *
 * See http://www.asterisk.org for more information about
 * the Asterisk project. Please do not directly contact
 * any of the maintainers of this project for assistance;
 * the project provides a web site, mailing lists and IRC
 * channels for your use.
 *
 * This program is free software, distributed under the terms of
 * the GNU General Public License Version 2. See the LICENSE file
 * at the top of the source tree.
 */

/*! \file
 * \brief Sorcery Data Access Layer API
 * \author Joshua Colp <jcolp@digium.com>
 * \ref AstSorcery
 */

/*!
 * \page AstSorcery Data Access Layer API
 *
 * Sorcery is a unifying data access layer which utilizes the configuration framework,
 * realtime, and astdb to allow object creation, retrieval, updating, and deletion.
 *
 * \par Initialization
 *
 * Usage of sorcery is accomplished by first opening a sorcery structure. This structure holds
 * all information about the object types, object fields, and object mappings. All API functions
 * require the sorcery structure to operate. When sorcery is no longer needed the structure can
 * be unreferenced using \ref ast_sorcery_unref
 *
 * Once opened the sorcery structure must have object mappings applied to it. This maps the
 * object types to their respective wizards (object storage modules). If the developer would like
 * to allow the user to configure this using the sorcery.conf configuration file the
 * \ref ast_sorcery_apply_config API call can be used to read in the configuration file and apply the
 * mappings. If the storage of the object types are such that a default wizard can be used this can
 * be applied using the \ref ast_sorcery_apply_default API call. Note that the default mappings will not
 * override configured mappings. They are only used in the case where no configured mapping exists.
 *
 * Configuring object mappings implicitly creates a basic version of an object type. The object type
 * must be fully registered, however, using the \ref ast_sorcery_object_register API call before any
 * objects of the type can be allocated, created, or retrieved.
 *
 * Once the object type itself has been fully registered the individual fields within the object must
 * be registered using the \ref ast_sorcery_object_field_register API call. Note that not all fields *need*
 * be registered. Only fields that should be accessible using the sorcery API have to be registered.
 *
 * \par Creating Objects
 *
 * Before an object can be created within the sorcery API it must first be allocated using the
 * \ref ast_sorcery_alloc API call. This allocates a new instance of the object, sets sorcery specific
 * details, and applies default values to the object. A unique identifier can optionally be specified
 * when allocating an object. If it is not provided one will be automatically generated. Allocating
 * an object does not create it within any object storage mechanisms that are configured for the
 * object type. Creation must explicitly be done using the \ref ast_sorcery_create API call. This API call
 * passes the object to each configured object storage mechanism for the object type until one
 * successfully persists the object.
 *
 * \par Retrieving Objects
 *
 * To retrieve a single object using its unique identifier the \ref ast_sorcery_retrieve_by_id API call
 * can be used.
 *
 * To retrieve potentially multiple objects using specific fields the \ref ast_sorcery_retrieve_by_fields
 * API call can be used. The behavior of this API call is controlled using different flags. If the
 * AST_RETRIEVE_FLAG_MULTIPLE flag is used a container will be returned which contains all matching objects.
 * To retrieve all objects the AST_RETRIEVE_FLAG_ALL flag can be specified. Note that when specifying this flag
 * you do not need to pass any fields.
 *
 * Both API calls return shared objects. Modification of the object can not occur until it has been copied.
 *
 * \par Updating Objects
 *
 * As retrieved objects may be shared the first step to updating the object with new details is creating a
 * copy using the \ref ast_sorcery_copy API call. This will return a new object which is specific to the caller.
 * Any field within the object may be modified as needed. Once changes are done the changes can be committed
 * using the \ref ast_sorcery_update API call. Note that as the copied object is specific to the caller it must
 * be unreferenced after use.
 *
 * \par Deleting Objects
 *
 * To delete an object simply call the \ref ast_sorcery_delete API call with an object retrieved using the
 * ast_sorcery_retrieve_by_* API calls or a copy returned from \ref ast_sorcery_copy.
 */

#ifndef _ASTERISK_SORCERY_H
#define _ASTERISK_SORCERY_H

#if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif

#include "asterisk/config_options.h"
#include "asterisk/uuid.h"

/*! \brief Maximum size of an object type */
#define MAX_OBJECT_TYPE 64

/*!
 * \brief Retrieval flags
 */
enum ast_sorcery_retrieve_flags {
	/*! \brief Default retrieval flags */
	AST_RETRIEVE_FLAG_DEFAULT = 0,

	/*! \brief Return all matching objects */
	AST_RETRIEVE_FLAG_MULTIPLE = (1 << 0),

	/*! \brief Perform no matching, return all objects */
	AST_RETRIEVE_FLAG_ALL = (1 << 1),
};

/*! \brief Forward declaration for the sorcery main structure */
struct ast_sorcery;

/*!
 * \brief A callback function for translating a value into a string
 *
 * \param obj Object to get value from
 * \param args Where the field is
 * \param buf Pointer to the buffer that the handler has created which contains the field value
 *
 * \retval 0 success
 * \retval -1 failure
 */
typedef int (*sorcery_field_handler)(const void *obj, const intptr_t *args, char **buf);

/*!
 * \brief A callback function for translating multiple values into an ast_variable list
 *
 * \param obj Object to get values from
 * \param fields Pointer to store the list of fields
 *
 * \retval 0 success
 * \retval -1 failure
 */
typedef int (*sorcery_fields_handler)(const void *obj, struct ast_variable **fields);

/*!
 * \brief A callback function for performing a transformation on an object set
 *
 * \param set The existing object set
 *
 * \retval non-NULL new object set if changed
 * \retval NULL if no changes present
 *
 * \note The returned ast_variable list must be *new*. You can not return the input set.
 */
typedef struct ast_variable *(*sorcery_transform_handler)(struct ast_variable *set);

/*!
 * \brief A callback function for when an object set is successfully applied to an object
 *
 * \note On a failure return, the state of the object is left undefined. It is a bad
 * idea to try to use this object.
 *
 * \param sorcery Sorcery structure in use
 * \param obj The object itself
 * \retval 0 Success
 * \retval non-zero Failure
 */
typedef int (*sorcery_apply_handler)(const struct ast_sorcery *sorcery, void *obj);

/*!
 * \brief A callback function for copying the contents of one object to another
 *
 * \param src The source object
 * \param dst The destination object
 *
 * \retval 0 success
 * \retval -1 failure
 */
typedef int (*sorcery_copy_handler)(const void *src, void *dst);

/*!
 * \brief A callback function for generating a changeset between two objects
 *
 * \param original The original object
 * \param modified The modified object
 * \param changes The changeset
 *
 * \param 0 success
 * \param -1 failure
 */
typedef int (*sorcery_diff_handler)(const void *original, const void *modified, struct ast_variable **changes);

/*! \brief Interface for a sorcery wizard */
struct ast_sorcery_wizard {
	/*! \brief Name of the wizard */
	const char *name;

	/*! \brief Pointer to the Asterisk module this wizard is implemented by */
	struct ast_module *module;

	/*! \brief Callback for opening a wizard */
	void *(*open)(const char *data);

	/*! \brief Optional callback for loading persistent objects */
	void (*load)(void *data, const struct ast_sorcery *sorcery, const char *type);

	/*! \brief Optional callback for reloading persistent objects */
	void (*reload)(void *data, const struct ast_sorcery *sorcery, const char *type);

	/*! \brief Callback for creating an object */
	int (*create)(const struct ast_sorcery *sorcery, void *data, void *object);

	/*! \brief Callback for retrieving an object using an id */
	void *(*retrieve_id)(const struct ast_sorcery *sorcery, void *data, const char *type, const char *id);

	/*! \brief Callback for retrieving multiple objects using a regex on their id */
	void (*retrieve_regex)(const struct ast_sorcery *sorcery, void *data, const char *type, struct ao2_container *objects, const char *regex);

	/*! \brief Optional callback for retrieving an object using fields */
	void *(*retrieve_fields)(const struct ast_sorcery *sorcery, void *data, const char *type, const struct ast_variable *fields);

	/*! \brief Optional callback for retrieving multiple objects using some optional field criteria */
	void (*retrieve_multiple)(const struct ast_sorcery *sorcery, void *data, const char *type, struct ao2_container *objects, const struct ast_variable *fields);

	/*! \brief Callback for updating an object */
	int (*update)(const struct ast_sorcery *sorcery, void *data, void *object);

	/*! \brief Callback for deleting an object */
	int (*delete)(const struct ast_sorcery *sorcery, void *data, void *object);

	/*! \brief Callback for closing a wizard */
	void (*close)(void *data);
};

/*! \brief Interface for a sorcery object type observer */
struct ast_sorcery_observer {
	/*! \brief Callback for when an object is created */
	void (*created)(const void *object);

	/*! \brief Callback for when an object is updated */
	void (*updated)(const void *object);

	/*! \brief Callback for when an object is deleted */
	void (*deleted)(const void *object);

	/*! \brief Callback for when an object type is loaded/reloaded */
	void (*loaded)(const char *object_type);
};

/*! \brief Opaque structure for internal sorcery object */
struct ast_sorcery_object;

/*! \brief Structure which contains details about a sorcery object */
struct ast_sorcery_object_details {
	/*! \brief Pointer to internal sorcery object information */
	struct ast_sorcery_object *object;
};

/*! \brief Macro which must be used at the beginning of each sorcery capable object */
#define SORCERY_OBJECT(details)                    \
struct {                                           \
	struct ast_sorcery_object_details details; \
}                                                  \

/*!
 * \brief Initialize the sorcery API
 *
 * \retval 0 success
 * \retval -1 failure
 */
int ast_sorcery_init(void);

/*!
 * \brief Register a sorcery wizard
 *
 * \param interface Pointer to a wizard interface
 * \param module Pointer to the module implementing the interface
 *
 * \retval 0 success
 * \retval -1 failure
 */
int __ast_sorcery_wizard_register(const struct ast_sorcery_wizard *interface, struct ast_module *module);

/*!
 * \brief See \ref __ast_sorcery_wizard_register()
 */
#define ast_sorcery_wizard_register(interface) __ast_sorcery_wizard_register(interface, ast_module_info ? ast_module_info->self : NULL)

/*!
 * \brief Unregister a sorcery wizard
 *
 * \param interface Pointer to the wizard interface
 *
 * \retval 0 success
 * \retval -1 failure
 */
int ast_sorcery_wizard_unregister(const struct ast_sorcery_wizard *interface);

/*!
 * \brief Open a new sorcery structure
 *
 * \retval non-NULL success
 * \retval NULL if allocation failed
 */
struct ast_sorcery *ast_sorcery_open(void);

/*!
 * \brief Apply configured wizard mappings
 *
 * \param sorcery Pointer to a sorcery structure
 * \param name Name of the category to use within the configuration file, normally the module name
 * \param module The module name (AST_MODULE)
 *
 * \retval 0 success
 * \retval -1 failure
 */
int __ast_sorcery_apply_config(struct ast_sorcery *sorcery, const char *name, const char *module);

#define ast_sorcery_apply_config(sorcery, name) \
	__ast_sorcery_apply_config((sorcery), (name), AST_MODULE)

/*!
 * \brief Apply default object wizard mappings
 *
 * \param sorcery Pointer to a sorcery structure
 * \param type Type of object to apply to
 * \param module The name of the module, typically AST_MODULE
 * \param name Name of the wizard to use
 * \param data Data to be passed to wizard
 *
 * \retval 0 success
 * \retval -1 failure
 *
 * \note This should be called *after* applying configuration sourced mappings
 *
 * \note Only a single default can exist per object type
 */
int __ast_sorcery_apply_default(struct ast_sorcery *sorcery, const char *type, const char *module, const char *name, const char *data);

#define ast_sorcery_apply_default(sorcery, type, name, data) \
	__ast_sorcery_apply_default((sorcery), (type), AST_MODULE, (name), (data))

/*!
 * \brief Register an object type
 *
 * \param sorcery Pointer to a sorcery structure
 * \param type Type of object
 * \param hidden All objects of this type are internal and should not be manipulated by users
 * \param alloc Required object allocation callback
 * \param transform Optional transformation callback
 * \param apply Optional object set apply callback
 *
 * \note In general, this function should not be used directly. One of the various
 * macro'd versions should be used instead.
 *
 * \retval 0 success
 * \retval -1 failure
 */
int __ast_sorcery_object_register(struct ast_sorcery *sorcery, const char *type, unsigned int hidden, unsigned int reloadable, aco_type_item_alloc alloc, sorcery_transform_handler transform, sorcery_apply_handler apply);

/*!
 * \brief Register an object type
 *
 * \param sorcery Pointer to a sorcery structure
 * \param type Type of object
 * \param alloc Required object allocation callback
 * \param transform Optional transformation callback
 * \param apply Optional object set apply callback
 *
 * \retval 0 success
 * \retval -1 failure
 */
#define ast_sorcery_object_register(sorcery, type, alloc, transform, apply) \
	__ast_sorcery_object_register((sorcery), (type), 0, 1, (alloc), (transform), (apply))

/*!
 * \brief Register an object type that is not reloadable
 *
 * \param sorcery Pointer to a sorcery structure
 * \param type Type of object
 * \param alloc Required object allocation callback
 * \param transform Optional transformation callback
 * \param apply Optional object set apply callback
 *
 * \retval 0 success
 * \retval -1 failure
 */
#define ast_sorcery_object_register_no_reload(sorcery, type, alloc, transform, apply) \
	__ast_sorcery_object_register((sorcery), (type), 0, 0, (alloc), (transform), (apply))

/*!
 * \brief Register an internal, hidden object type
 *
 * \param sorcery Pointer to a sorcery structure
 * \param type Type of object
 * \param alloc Required object allocation callback
 * \param transform Optional transformation callback
 * \param apply Optional object set apply callback
 *
 * \retval 0 success
 * \retval -1 failure
 */
#define ast_sorcery_internal_object_register(sorcery, type, alloc, transform, apply) \
	__ast_sorcery_object_register((sorcery), (type), 1, 1, (alloc), (transform), (apply))

/*!
 * \brief Set the copy handler for an object type
 *
 * \param sorcery Pointer to a sorcery structure
 * \param type Type of object
 * \param copy Copy handler
 */
void ast_sorcery_object_set_copy_handler(struct ast_sorcery *sorcery, const char *type, sorcery_copy_handler copy);

/*!
 * \brief Set the diff handler for an object type
 *
 * \param sorcery Pointer to a sorcery structure
 * \param type Type of object
 * \param diff Diff handler
 */
void ast_sorcery_object_set_diff_handler(struct ast_sorcery *sorcery, const char *type, sorcery_diff_handler diff);

/*!
 * \brief Register a regex for multiple fields within an object
 *
 * \param sorcery Pointer to a sorcery structure
 * \param type Type of object
 * \param regex A regular expression pattern for the fields
 * \param config_handler A custom handler for translating the string representation of the fields
 * \param sorcery_handler A custom handler for translating the native representation of the fields
 *
 * \retval 0 success
 * \retval -1 failure
 */
int ast_sorcery_object_fields_register(struct ast_sorcery *sorcery, const char *type, const char *regex, aco_option_handler config_handler,
									   sorcery_fields_handler sorcery_handler);

/*!
 * \brief Register a field within an object
 *
 * \param sorcery Pointer to a sorcery structure
 * \param type Type of object
 * \param name Name of the field
 * \param default_val Default value of the field
 * \param opt_type Option type
 * \param flags Option type specific flags
 *
 * \retval 0 success
 * \retval -1 failure
 */
int __ast_sorcery_object_field_register(struct ast_sorcery *sorcery, const char *type, const char *name, const char *default_val, enum aco_option_type opt_type,
                                        aco_option_handler config_handler, sorcery_field_handler sorcery_handler, unsigned int flags, unsigned int no_doc,
                                        size_t argc, ...);

/*!
 * \brief Register a field within an object
 *
 * \param sorcery Pointer to a sorcery structure
 * \param type Type of object
 * \param name Name of the field
 * \param default_val Default value of the field
 * \param opt_type Option type
 * \param flags Option type specific flags
 *
 * \retval 0 success
 * \retval -1 failure
 */
#define ast_sorcery_object_field_register(sorcery, type, name, default_val, opt_type, flags, ...) \
    __ast_sorcery_object_field_register(sorcery, type, name, default_val, opt_type, NULL, NULL, flags, 0, VA_NARGS(__VA_ARGS__), __VA_ARGS__)

/*!
 * \brief Register a field within an object without documentation
 *
 * \param sorcery Pointer to a sorcery structure
 * \param type Type of object
 * \param name Name of the field
 * \param default_val Default value of the field
 * \param opt_type Option type
 * \param flags Option type specific flags
 *
 * \retval 0 success
 * \retval -1 failure
 */
#define ast_sorcery_object_field_register_nodoc(sorcery, type, name, default_val, opt_type, flags, ...) \
    __ast_sorcery_object_field_register(sorcery, type, name, default_val, opt_type, NULL, NULL, flags, 1, VA_NARGS(__VA_ARGS__), __VA_ARGS__)

/*!
 * \brief Register a field within an object with custom handlers
 *
 * \param sorcery Pointer to a sorcery structure
 * \param type Type of object
 * \param name Name of the field
 * \param default_val Default value of the field
 * \param config_handler Custom configuration handler
 * \param sorcery_handler Custom sorcery handler
 * \param flags Option type specific flags
 *
 * \retval 0 success
 * \retval -1 failure
 */
#define ast_sorcery_object_field_register_custom(sorcery, type, name, default_val, config_handler, sorcery_handler, flags, ...) \
    __ast_sorcery_object_field_register(sorcery, type, name, default_val, OPT_CUSTOM_T, config_handler, sorcery_handler, flags, 0, VA_NARGS(__VA_ARGS__), __VA_ARGS__);

/*!
 * \brief Register a field within an object with custom handlers without documentation
 *
 * \param sorcery Pointer to a sorcery structure
 * \param type Type of object
 * \param name Name of the field
 * \param default_val Default value of the field
 * \param config_handler Custom configuration handler
 * \param sorcery_handler Custom sorcery handler
 * \param flags Option type specific flags
 *
 * \retval 0 success
 * \retval -1 failure
 */
#define ast_sorcery_object_field_register_custom_nodoc(sorcery, type, name, default_val, config_handler, sorcery_handler, flags, ...) \
    __ast_sorcery_object_field_register(sorcery, type, name, default_val, OPT_CUSTOM_T, config_handler, sorcery_handler, flags, 1, VA_NARGS(__VA_ARGS__), __VA_ARGS__);

/*!
 * \brief Inform any wizards to load persistent objects
 *
 * \param sorcery Pointer to a sorcery structure
 */
void ast_sorcery_load(const struct ast_sorcery *sorcery);

/*!
 * \brief Inform any wizards of a specific object type to load persistent objects
 *
 * \param sorcery Pointer to a sorcery structure
 * \param type Name of the object type to load
 */
void ast_sorcery_load_object(const struct ast_sorcery *sorcery, const char *type);

/*!
 * \brief Inform any wizards to reload persistent objects
 *
 * \param sorcery Pointer to a sorcery structure
 */
void ast_sorcery_reload(const struct ast_sorcery *sorcery);

/*!
 * \brief Inform any wizards of a specific object type to reload persistent objects
 *
 * \param sorcery Pointer to a sorcery structure
 * \param type Name of the object type to reload
 */
void ast_sorcery_reload_object(const struct ast_sorcery *sorcery, const char *type);

/*!
 * \brief Increase the reference count of a sorcery structure
 *
 * \param sorcery Pointer to a sorcery structure
 */
void ast_sorcery_ref(struct ast_sorcery *sorcery);

/*!
 * \brief Create an object set (KVP list) for an object
 *
 * \param sorcery Pointer to a sorcery structure
 * \param object Pointer to a sorcery object
 *
 * \retval non-NULL success
 * \retval NULL if error occurred
 *
 * \note The returned ast_variable list must be destroyed using ast_variables_destroy
 */
struct ast_variable *ast_sorcery_objectset_create(const struct ast_sorcery *sorcery, const void *object);

/*!
 * \brief Create an object set in JSON format for an object
 *
 * \param sorcery Pointer to a sorcery structure
 * \param object Pointer to a sorcery object
 *
 * \retval non-NULL success
 * \retval NULL if error occurred
 *
 * \note The returned ast_json object must be unreferenced using ast_json_unref
 */
struct ast_json *ast_sorcery_objectset_json_create(const struct ast_sorcery *sorcery, const void *object);

/*!
 * \brief Apply an object set (KVP list) to an object
 *
 * \param sorcery Pointer to a sorcery structure
 * \param object Pointer to a sorcery object
 * \param objectset Object set itself
 *
 * \retval 0 success
 * \retval -1 failure
 *
 * \note This operation is *not* atomic. If this fails it is possible for the object to be left with a partially
 *       applied object set.
 */
int ast_sorcery_objectset_apply(const struct ast_sorcery *sorcery, void *object, struct ast_variable *objectset);

/*!
 * \brief Create a changeset given two object sets
 *
 * \param original Original object set
 * \param modified Modified object set
 * \param changes Pointer to hold any changes between the object sets
 *
 * \retval 0 success
 * \retval -1 failure
 *
 * \note The returned ast_variable list must be destroyed using ast_variables_destroy
 */
int ast_sorcery_changeset_create(const struct ast_variable *original, const struct ast_variable *modified, struct ast_variable **changes);

/*!
 * \brief Allocate a generic sorcery capable object
 *
 * \param size Size of the object
 * \param destructor Optional destructor function
 *
 * \retval non-NULL success
 * \retval NULL failure
 */
void *ast_sorcery_generic_alloc(size_t size, ao2_destructor_fn destructor);

/*!
 * \brief Allocate an object
 *
 * \param sorcery Pointer to a sorcery structure
 * \param type Type of object to allocate
 * \param id Optional unique identifier, if none is provided one will be generated
 *
 * \retval non-NULL success
 * \retval NULL failure
 */
void *ast_sorcery_alloc(const struct ast_sorcery *sorcery, const char *type, const char *id);

/*!
 * \brief Create a copy of an object
 *
 * \param sorcery Pointer to a sorcery structure
 * \param object Existing object
 *
 * \retval non-NULL success
 * \retval NULL failure
 */
void *ast_sorcery_copy(const struct ast_sorcery *sorcery, const void *object);

/*!
 * \brief Create a changeset of two objects
 *
 * \param sorcery Pointer to a sorcery structure
 * \param original Original object
 * \param modified Modified object
 * \param changes Pointer which will be populated with changes if any exist
 *
 * \retval 0 success
 * \retval -1 failure
 *
 * \note The returned ast_variable list must be destroyed using ast_variables_destroy
 *
 * \note While the objects must be of the same type they do not have to be the same object
 */
int ast_sorcery_diff(const struct ast_sorcery *sorcery, const void *original, const void *modified, struct ast_variable **changes);

/*!
 * \brief Add an observer to a specific object type
 *
 * \param sorcery Pointer to a sorcery structure
 * \param type Type of object that should be observed
 * \param callbacks Implementation of the observer interface
 *
 * \retval 0 success
 * \retval -1 failure
 *
 * \note You must be ready to accept observer invocations before this function is called
 */
int ast_sorcery_observer_add(const struct ast_sorcery *sorcery, const char *type, const struct ast_sorcery_observer *callbacks);

/*!
 * \brief Remove an observer from a specific object type
 *
 * \param sorcery Pointer to a sorcery structure
 * \param type Type of object that should no longer be observed
 * \param callbacks Implementation of the observer interface
 *
 * \retval 0 success
 * \retval -1 failure
 */
void ast_sorcery_observer_remove(const struct ast_sorcery *sorcery, const char *type, struct ast_sorcery_observer *callbacks);

/*!
 * \brief Create and potentially persist an object using an available wizard
 *
 * \param sorcery Pointer to a sorcery structure
 * \param object Pointer to a sorcery object
 *
 * \retval 0 success
 * \retval -1 failure
 */
int ast_sorcery_create(const struct ast_sorcery *sorcery, void *object);

/*!
 * \brief Retrieve an object using its unique identifier
 *
 * \param sorcery Pointer to a sorcery structure
 * \param type Type of object to retrieve
 * \param id Unique object identifier
 *
 * \retval non-NULL if found
 * \retval NULL if not found
 */
void *ast_sorcery_retrieve_by_id(const struct ast_sorcery *sorcery, const char *type, const char *id);

/*!
 * \brief Retrieve an object or multiple objects using specific fields
 *
 * \param sorcery Pointer to a sorcery structure
 * \param type Type of object to retrieve
 * \param flags Flags to control behavior
 * \param fields Optional jbject fields and values to match against
 *
 * \retval non-NULL if found
 * \retval NULL if not found
 *
 * \note If the AST_RETRIEVE_FLAG_MULTIPLE flag is specified the returned value will be an
 *       ao2_container that must be unreferenced after use.
 *
 * \note If the AST_RETRIEVE_FLAG_ALL flag is used you may omit fields to retrieve all objects
 *       of the given type.
 */
void *ast_sorcery_retrieve_by_fields(const struct ast_sorcery *sorcery, const char *type, unsigned int flags, struct ast_variable *fields);

/*!
 * \brief Retrieve multiple objects using a regular expression on their id
 *
 * \param sorcery Pointer to a sorcery structure
 * \param type Type of object to retrieve
 * \param regex Regular expression
 *
 * \retval non-NULL if error occurs
 * \retval NULL success
 *
 * \note The provided regex is treated as extended case sensitive.
 */
struct ao2_container *ast_sorcery_retrieve_by_regex(const struct ast_sorcery *sorcery, const char *type, const char *regex);

/*!
 * \brief Update an object
 *
 * \param sorcery Pointer to a sorcery structure
 * \param object Pointer to a sorcery object
 *
 * \retval 0 success
 * \retval -1 failure
 */
int ast_sorcery_update(const struct ast_sorcery *sorcery, void *object);

/*!
 * \brief Delete an object
 *
 * \param sorcery Pointer to a sorcery structure
 * \param object Pointer to a sorcery object
 *
 * \retval 0 success
 * \retval -1 failure
 */
int ast_sorcery_delete(const struct ast_sorcery *sorcery, void *object);

/*!
 * \brief Decrease the reference count of a sorcery structure
 *
 * \param sorcery Pointer to a sorcery structure
 */
void ast_sorcery_unref(struct ast_sorcery *sorcery);

/*!
 * \brief Get the unique identifier of a sorcery object
 *
 * \param object Pointer to a sorcery object
 *
 * \retval unique identifier
 */
const char *ast_sorcery_object_get_id(const void *object);

/*!
 * \brief Get the type of a sorcery object
 *
 * \param object Pointer to a sorcery object
 *
 * \retval type of object
 */
const char *ast_sorcery_object_get_type(const void *object);

/*!
 * \brief Get an extended field value from a sorcery object
 *
 * \param object Pointer to a sorcery object
 * \param name Name of the extended field value
 *
 * \retval non-NULL if found
 * \retval NULL if not found
 *
 * \note The returned string does NOT need to be freed and is guaranteed to remain valid for the lifetime of the object
 */
const char *ast_sorcery_object_get_extended(const void *object, const char *name);

/*!
 * \brief Set an extended field value on a sorcery object
 *
 * \param object Pointer to a sorcery object
 * \param name Name of the extended field
 * \param value Value of the extended field
 *
 * \retval 0 success
 * \retval -1 failure
 *
 * \note The field name MUST begin with '@' to indicate it is an extended field.
 * \note If the extended field already exists it will be overwritten with the new value.
 */
int ast_sorcery_object_set_extended(const void *object, const char *name, const char *value);

#if defined(__cplusplus) || defined(c_plusplus)
}
#endif

#endif /* _ASTERISK_SORCERY_H */
