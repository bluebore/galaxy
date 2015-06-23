"""
Django settings for backend project.

For more information on this file, see
https://docs.djangoproject.com/en/1.7/topics/settings/

For the full list of settings and their values, see
https://docs.djangoproject.com/en/1.7/ref/settings/
"""

# Build paths inside the project like this: os.path.join(BASE_DIR, ...)
import os
BASE_DIR = os.path.dirname(os.path.dirname(__file__))


# Quick-start development settings - unsuitable for production
# See https://docs.djangoproject.com/en/1.7/howto/deployment/checklist/

# SECURITY WARNING: keep the secret key used in production secret!
SECRET_KEY = 'pme%=52e+c=_n%gau$6h-=w@+^6l4r8d*-x9a*q5a$ixsk7(^b'

# SECURITY WARNING: don't run with debug turned on in production!
DEBUG = True

TEMPLATE_DEBUG = True

ALLOWED_HOSTS = ['*']


# Application definition

INSTALLED_APPS = (
    'django.contrib.admin',
    'django.contrib.auth',
    'django.contrib.contenttypes',
    'django.contrib.sessions',
    'django.contrib.messages',
    'django.contrib.staticfiles',
    'bootstrap'
)

MIDDLEWARE_CLASSES = (
    'django.contrib.sessions.middleware.SessionMiddleware',
    'django.middleware.common.CommonMiddleware',
    'django.middleware.csrf.CsrfViewMiddleware',
    'django.contrib.auth.middleware.AuthenticationMiddleware',
    'django.contrib.auth.middleware.SessionAuthenticationMiddleware',
    'django.contrib.messages.middleware.MessageMiddleware',
    'django.middleware.clickjacking.XFrameOptionsMiddleware',
)

ROOT_URLCONF = 'bootstrap.urls'



# Database
# https://docs.djangoproject.com/en/1.7/ref/settings/#databases

DATABASES = {
  'default': {
        'ENGINE': 'django.db.backends.mysql',
        'NAME': 'galaxy_console',
        'USER': 'root',
        'PASSWORD': 'galaxy',
        'HOST': 'cq01-rdqa-pool056.cq01.baidu.com',
        'PORT': '3306'
    }        
}

TEMPLATE_DIRS = (
    os.path.join(BASE_DIR,'templates'),
)
# Internationalization
# https://docs.djangoproject.com/en/1.7/topics/i18n/

LANGUAGE_CODE = 'zh_CN'

TIME_ZONE = 'Asia/Shanghai'

USE_I18N = True

USE_L10N = True

USE_TZ = True


# Static files (CSS, JavaScript, Images)
# https://docs.djangoproject.com/en/1.7/howto/static-files/
STATIC_URL = '/statics/'

LOGGING = {
    'version': 1,
    'disable_existing_loggers': False,
    'handlers': {
        'console': {
            'class': 'logging.StreamHandler',
        },
    },
    'loggers': {
        'console': {
            'handlers': ['console'],
            'level': os.getenv('DJANGO_LOG_LEVEL', 'INFO'),
        },
    },
}
STATICFILES_DIRS = (
   os.path.join(BASE_DIR, "statics"),
)
AUTHENTICATION_BACKENDS = ('common.cas.UUAPBackend',)

GALAXY_MASTER='localhost:8102'
GALAXY_CLIENT_BIN='/home/users/wangtaize/workspace/ps/se/galaxybk/galaxy_client'
UUAP_CAS_SERVER='http://itebeta.baidu.com:8100/login'
MY_HOST='http://cq01-rdqa-pool056.cq01.baidu.com:8989/'
UUAP_VALIDATE_URL='http://itebeta.baidu.com:8100/serviceValidate'
UIC_SERVICE="http://uuap.baidu.com:8086/ws/UserRemoteService?wsdl"
UIC_KEY="uuapclient-18-4x4g9aNftempiJaHnelz"

